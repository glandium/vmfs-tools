/* 
 * VMFS file abstraction.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "vmfs.h"

/* Create a file structure */
vmfs_file_t *vmfs_file_create_struct(vmfs_volume_t *vol)
{
   vmfs_file_t *f;

   if (!(f = calloc(1,sizeof(*f))))
      return NULL;

   f->vol = vol;
   return f;
}

/* Open a file based on a directory entry */
vmfs_file_t *vmfs_file_open_rec(vmfs_fs_t *fs,vmfs_dirent_t *rec)
{
   u_char buf[VMFS_INODE_SIZE];
   vmfs_file_t *f;

   if (!(f = vmfs_file_create_struct(fs->vol)))
      return NULL;
   
   /* Read the inode */
   if (vmfs_inode_get(fs->vol,rec,buf) == -1) {
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
vmfs_file_t *vmfs_file_open(vmfs_fs_t *fs,char *filename)
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

/* Read data from a file */
ssize_t vmfs_file_read(vmfs_file_t *f,u_char *buf,size_t len)
{
   vmfs_bitmap_header_t *sbc_bmh;
   vmfs_file_t *sbc;
   u_int blk_pos;
   m_u32_t blk_id,blk_type;
   m_u64_t blk_size,blk_len;
   m_u64_t file_size,offset;
   ssize_t res,rlen = 0;
   size_t clen,exp_len;

   blk_size = vmfs_vol_get_blocksize(f->vol);
   file_size = vmfs_file_get_size(f);

   sbc = f->vol->sbc;
   sbc_bmh = &f->vol->sbc_bmh;

   while(len > 0) {
      blk_pos = f->pos / blk_size;

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
            offset = f->pos % blk_size;
            blk_len = blk_size - offset;
            exp_len = m_min(blk_len,len);
            res = m_min(exp_len,file_size - f->pos);
            memset(buf,0,res);
            break;

         /* Full-Block */
         case VMFS_BLK_TYPE_FB:
            offset = f->pos % blk_size;
            blk_len = blk_size - offset;
            exp_len = m_min(blk_len,len);
            clen = m_min(exp_len,file_size - f->pos);

#if 0
            printf("vmfs_file_read: f->pos=0x%llx, offset=0x%8.8llx\n",
                   (m_u64_t)f->pos,offset);
#endif

            res = vmfs_vol_read(f->vol,VMFS_BLK_FB_NUMBER(blk_id),offset,
                                buf,clen);
            break;

         /* Sub-Block */
         case VMFS_BLK_TYPE_SB: {
            m_u32_t sbc_subgroup,sbc_number,sbc_blk;
            off_t sbc_addr;

            offset = f->pos % sbc_bmh->data_size;
            blk_len = sbc_bmh->data_size - offset;
            exp_len = m_min(blk_len,len);
            clen = m_min(exp_len,file_size - f->pos);

            sbc_subgroup = VMFS_BLK_SB_SUBGROUP(blk_id);
            sbc_number   = VMFS_BLK_SB_NUMBER(blk_id);

            sbc_blk = sbc_number * sbc_bmh->items_per_bitmap_entry;
            sbc_blk += sbc_subgroup;

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
      f->pos += res;
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

/* Dump a file */
int vmfs_file_dump(vmfs_file_t *f,off_t pos,size_t len,FILE *fd_out)
{
   u_char *buf;
   ssize_t res;
   size_t clen,buf_len;

   if (!len)
      len = vmfs_file_get_size(f);

   buf_len = 0x100000;

   if (!(buf = malloc(buf_len)))
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
int vmfs_file_fstat(vmfs_file_t *f,struct stat *buf)
{
   return(vmfs_inode_stat(&f->inode,buf));
}

/* Get file status (similar to fstat(), but with a path) */
static int vmfs_file_stat_internal(vmfs_fs_t *fs,char *path,
                                   int follow_symlink,
                                   struct stat *buf)
{
   u_char inode_buf[VMFS_INODE_SIZE];
   vmfs_dirent_t entry;
   vmfs_inode_t inode;

   if (vmfs_dirent_resolve_path(fs,fs->root_dir,path,follow_symlink,
                                &entry) != 1)
      return(-1);

   if (vmfs_inode_get(fs->vol,&entry,inode_buf) == -1)
      return(-1);
   
   vmfs_inode_read(&inode,inode_buf);
   vmfs_inode_stat(&inode,buf);
   return(0);
}

/* Get file file status (follow symlink) */
int vmfs_file_stat(vmfs_fs_t *fs,char *path,struct stat *buf)
{
   return(vmfs_file_stat_internal(fs,path,1,buf));
}

/* Get file file status (do not follow symlink) */
int vmfs_file_lstat(vmfs_fs_t *fs,char *path,struct stat *buf)
{   
   return(vmfs_file_stat_internal(fs,path,0,buf));
}
