/*
 * vmfs-tools - Tools to access VMFS filesystems
 * Copyright (C) 2009 Christophe Fillot <cf@utc.fr>
 * Copyright (C) 2009 Mike Hommey <mh@glandium.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/* 
 * VMFS file abstraction.
 */
#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "vmfs.h"

/* Create a file structure */
vmfs_file_t *vmfs_file_create_struct(const vmfs_fs_t *fs)
{
   vmfs_file_t *f;

   if (!(f = calloc(1,sizeof(*f))))
      return NULL;

   f->fs = fs;
   return f;
}

/* Open a file based on a directory entry */
vmfs_file_t *vmfs_file_open_rec(const vmfs_fs_t *fs,const vmfs_dirent_t *rec)
{
   DECL_ALIGNED_BUFFER_WOL(buf,VMFS_INODE_SIZE);
   vmfs_file_t *f;

   if (!(f = vmfs_file_create_struct(fs)))
      return NULL;
   
   /* Read the inode */
   if (vmfs_inode_get(fs,rec,buf) == -1) {
      fprintf(stderr,"VMFS: Unable to get inode info for dir entry '%s'\n",
              rec->name);
      return NULL;
   }

   /* Bind the associated inode */
   if (vmfs_inode_bind(f,buf) == -1)
      return NULL;

   return f;
}

/* Open a file */
vmfs_file_t *vmfs_file_open(const vmfs_fs_t *fs,const char *filename)
{
   vmfs_dirent_t rec;
   char *tmp_name;
   int res;

   if (!(tmp_name = strdup(filename)))
      return NULL;

   res = vmfs_dirent_resolve_path(fs,fs->root_dir,tmp_name,1,&rec);
   free(tmp_name);

   if (res != 1)
      return NULL;

   return(vmfs_file_open_rec(fs,&rec));
}

/* Close a file */
int vmfs_file_close(vmfs_file_t *f)
{
   if (f == NULL)
      return(-1);

   /* Free the block list */
   vmfs_blk_list_free(&f->blk_list);

   free(f);
   return(0);
}

/* Set position */
int vmfs_file_seek(vmfs_file_t *f,off_t pos,int whence)
{
   switch(whence) {
      case SEEK_SET:
         f->pos = pos;
         break;
      case SEEK_CUR:
         f->pos += pos;
         break;
      case SEEK_END:
         f->pos -= pos;
         break;
   }
   
   /* Normalize */
   if (f->pos < 0)
      f->pos = 0;
   else {
      if (f->pos > f->inode.size)
         f->pos = f->inode.size;
   }

   return(0);
}

/* Read data from a file at the specified position */
ssize_t vmfs_file_readat(vmfs_file_t *f,off_t *pos,u_char *buf,size_t len)
{
   const vmfs_bitmap_header_t *sbc_bmh;
   vmfs_file_t *sbc;
   u_int blk_pos;
   uint32_t blk_id,blk_type;
   uint64_t blk_size,blk_len;
   uint64_t file_size,offset;
   ssize_t res,rlen = 0;
   size_t clen,exp_len;

   DECL_ALIGNED_BUFFER(tbuf,M_BLK_SIZE);

   /* We don't handle RDM files */
   if (f->inode.type == VMFS_FILE_TYPE_RDM)
      return(-1);

   blk_size = vmfs_fs_get_blocksize(f->fs);
   file_size = vmfs_file_get_size(f);

   sbc = f->fs->sbc;
   sbc_bmh = &f->fs->sbc_bmh;

   while(len > 0) {
      blk_pos = *pos / blk_size;

      if (vmfs_blk_list_get_block(&f->blk_list,blk_pos,&blk_id) == -1)
         break;

#if 0
      if (f->vol->debug_level > 1)
         printf("vmfs_file_read: reading block 0x%8.8x\n",blk_id);
#endif

      blk_type = VMFS_BLK_TYPE(blk_id);

      switch(blk_type) {
         /* Copy-On-Write block */
         case VMFS_BLK_TYPE_COW:
            offset = *pos % blk_size;
            blk_len = blk_size - offset;
            exp_len = m_min(blk_len,len);
            res = m_min(exp_len,file_size - *pos);
            memset(buf,0,res);
            break;

         /* Full-Block */
         case VMFS_BLK_TYPE_FB: {
            off_t sub_page,sub_offset;

            offset = *pos % blk_size;

            exp_len = m_min(tbuf_len,len);
            clen = m_min(exp_len,file_size - *pos);

            sub_page   = offset & ~((off_t)tbuf_len - 1);
            sub_offset = offset & (tbuf_len - 1);

            /* 
             * If the user provided an aligned buffer acceptable for 
             * direct I/O, avoid a useless mem copy.
             */
            if (ALIGN_CHECK(sub_offset,M_DIO_BLK_SIZE) &&
                ALIGN_CHECK((uintptr_t)buf,M_DIO_BLK_SIZE) &&
                ALIGN_CHECK(clen,M_DIO_BLK_SIZE))
            {
               vmfs_fs_read(f->fs,VMFS_BLK_FB_NUMBER(blk_id),
                            sub_page+sub_offset,buf,clen);
            } else {
               vmfs_fs_read(f->fs,VMFS_BLK_FB_NUMBER(blk_id),sub_page,
                            tbuf,tbuf_len);
               memcpy(buf,tbuf+sub_offset,clen);
            }

            res = clen;
            break;
         }

         /* Sub-Block */
         case VMFS_BLK_TYPE_SB: {
            uint32_t sbc_subgroup,sbc_number,sbc_blk;
            off_t sbc_addr;

            offset = *pos % sbc_bmh->data_size;
            blk_len = sbc_bmh->data_size - offset;
            exp_len = m_min(blk_len,len);
            clen = m_min(exp_len,file_size - *pos);

            sbc_subgroup = VMFS_BLK_SB_SUBGROUP(blk_id);
            sbc_number   = VMFS_BLK_SB_NUMBER(blk_id);

            sbc_blk = sbc_subgroup * sbc_bmh->items_per_bitmap_entry;
            sbc_blk += sbc_number;

            sbc_addr = vmfs_bitmap_get_block_addr(sbc_bmh,sbc_blk);
            sbc_addr += offset;

            vmfs_file_seek(sbc,sbc_addr,SEEK_SET);
            res = vmfs_file_read(sbc,buf,clen);

            break;
         }

         default:
            fprintf(stderr,"VMFS: unknown block type 0x%2.2x\n",blk_type);
            return(-1);
      }

      /* Move file position and keep track of bytes currently read */
      *pos += res;
      rlen += res;

      /* Move buffer position */
      buf += res;
      len -= res;

      /* Incomplete read, stop now */
      if (res < exp_len)
         break;
   }

   return(rlen);
}

/* Read data from a file */
ssize_t vmfs_file_read(vmfs_file_t *f,u_char *buf,size_t len)
{
   return(vmfs_file_readat(f,&f->pos,buf,len));
}

/* Dump a file */
int vmfs_file_dump(vmfs_file_t *f,off_t pos,size_t len,FILE *fd_out)
{
   u_char *buf;
   ssize_t res;
   size_t clen,buf_len;

   if (!len)
      len = vmfs_file_get_size(f);

   buf_len = 0x100000;

   if (posix_memalign((void **)&buf,M_DIO_BLK_SIZE,buf_len) != 0)
      return(-1);

   vmfs_file_seek(f,pos,SEEK_SET);

   while(len > 0) {
      clen = m_min(len,buf_len);
      res = vmfs_file_read(f,buf,clen);

      if (res < 0) {
         fprintf(stderr,"vmfs_file_dump: problem reading input file.\n");
         return(-1);
      }

      if (fwrite(buf,1,res,fd_out) != res) {
         fprintf(stderr,"vmfs_file_dump: error writing output file.\n");
         return(-1);
      }

      if (res < clen)
         break;

      len -= res;
   }

   free(buf);
   return(0);
}

/* Get file status */
int vmfs_file_fstat(const vmfs_file_t *f,struct stat *buf)
{
   return(vmfs_inode_stat(&f->inode,buf));
}

/* Get file status (similar to fstat(), but with a path) */
static int vmfs_file_stat_internal(const vmfs_fs_t *fs,const char *path,
                                   int follow_symlink,
                                   struct stat *buf)
{
   DECL_ALIGNED_BUFFER_WOL(inode_buf,VMFS_INODE_SIZE);
   vmfs_dirent_t entry;
   vmfs_inode_t inode;

   vmfs_inode_update_mem(fs->root_dir);

   if (vmfs_dirent_resolve_path(fs,fs->root_dir,path,follow_symlink,
                                &entry) != 1)
      return(-1);

   if (vmfs_inode_get(fs,&entry,inode_buf) == -1)
      return(-1);
   
   vmfs_inode_read(&inode,inode_buf);
   vmfs_inode_stat(&inode,buf);
   return(0);
}

/* Get file file status (follow symlink) */
int vmfs_file_stat(const vmfs_fs_t *fs,const char *path,struct stat *buf)
{
   return(vmfs_file_stat_internal(fs,path,1,buf));
}

/* Get file file status (do not follow symlink) */
int vmfs_file_lstat(const vmfs_fs_t *fs,const char *path,struct stat *buf)
{   
   return(vmfs_file_stat_internal(fs,path,0,buf));
}
