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
#include <errno.h>
#include <sys/stat.h>
#include "vmfs.h"

/* Open a file based on an inode buffer */
vmfs_file_t *vmfs_file_open_from_inode(const vmfs_inode_t *inode)
{
   vmfs_file_t *f;

   if (!(f = calloc(1,sizeof(*f))))
      return NULL;

   f->inode = (vmfs_inode_t *)inode;
   return f;
}

/* Open a file based on a directory entry */
vmfs_file_t *vmfs_file_open_from_blkid(const vmfs_fs_t *fs,uint32_t blk_id)
{
   vmfs_inode_t *inode;

   if (!(inode = vmfs_inode_acquire(fs,blk_id)))
      return NULL;

   return(vmfs_file_open_from_inode(inode));
}

/* Open a file */
vmfs_file_t *vmfs_file_open_at(vmfs_dir_t *dir,const char *path)
{
   uint32_t blk_id;

   if (!(blk_id = vmfs_dir_resolve_path(dir,path,1)))
      return(NULL);

   return(vmfs_file_open_from_blkid(vmfs_dir_get_fs(dir),blk_id));
}

/* Create a new file entry */
int vmfs_file_create(vmfs_dir_t *d,const char *name,mode_t mode,
                     vmfs_inode_t **inode)
{      
   vmfs_fs_t *fs = (vmfs_fs_t *)vmfs_dir_get_fs(d);
   vmfs_inode_t *new_inode;
   int res;

   if (!vmfs_fs_readwrite(fs))
      return(-EROFS);

   if ((res = vmfs_inode_alloc(fs,VMFS_FILE_TYPE_FILE,mode,&new_inode)) < 0)
      return(res);

   if ((res = vmfs_dir_link_inode(d,name,new_inode)) < 0) {
      vmfs_block_free(fs,new_inode->id);
      vmfs_inode_release(new_inode);
      return(res);
   }

   *inode = new_inode;
   return(0);
}

/* Create a file */
vmfs_file_t *vmfs_file_create_at(vmfs_dir_t *dir,const char *path,mode_t mode)
{
   char *dir_name,*base_name;
   vmfs_dir_t *d = NULL;
   vmfs_file_t *f = NULL;
   vmfs_inode_t *inode;

   dir_name = m_dirname(path);
   base_name = m_basename(path);

   if (!dir_name || !base_name)
      goto done;

   if (!(d = vmfs_dir_open_at(dir,dir_name)))
      goto done;

   if (vmfs_file_create(d,base_name,mode,&inode) < 0)
      goto done;

   if (!(f = vmfs_file_open_from_inode(inode)))
      vmfs_inode_release(inode);

 done:
   vmfs_dir_close(d);
   free(dir_name);
   free(base_name);
   return f;
}

/* Close a file */
int vmfs_file_close(vmfs_file_t *f)
{
   if (f == NULL)
      return(-1);

   vmfs_inode_release(f->inode);
   free(f);
   return(0);
}

/* Read data from a file at the specified position */
ssize_t vmfs_file_pread(vmfs_file_t *f,u_char *buf,size_t len,off_t pos)
{
   const vmfs_fs_t *fs = vmfs_file_get_fs(f);
   uint32_t blk_id,blk_type;
   uint64_t blk_size,blk_len;
   uint64_t file_size,offset;
   ssize_t res=0,rlen = 0;
   size_t exp_len;
   int err;

   /* We don't handle RDM files */
   if (f->inode->type == VMFS_FILE_TYPE_RDM)
      return(-EIO);

   blk_size = vmfs_fs_get_blocksize(fs);
   file_size = vmfs_file_get_size(f);

   while(len > 0) {
      if (pos >= file_size)
         break;

      if ((err = vmfs_inode_get_block(f->inode,pos,&blk_id)) < 0)
         return(err);

#if 0
      if (f->vol->debug_level > 1)
         printf("vmfs_file_read: reading block 0x%8.8x\n",blk_id);
#endif

      blk_type = VMFS_BLK_FB_TBZ(blk_id) ?
                    VMFS_BLK_TYPE_NONE : VMFS_BLK_TYPE(blk_id);

      switch(blk_type) {
         /* Unallocated block */
         case VMFS_BLK_TYPE_NONE:
            offset = pos % blk_size;
            blk_len = blk_size - offset;
            exp_len = m_min(blk_len,len);
            res = m_min(exp_len,file_size - pos);
            memset(buf,0,res);
            break;

         /* File-Block */
         case VMFS_BLK_TYPE_FB:
            exp_len = m_min(len,file_size - pos);
            res = vmfs_block_read_fb(fs,blk_id,pos,buf,exp_len);
            break;

         /* Sub-Block */
         case VMFS_BLK_TYPE_SB: {
            exp_len = m_min(len,file_size - pos);
            res = vmfs_block_read_sb(fs,blk_id,pos,buf,exp_len);
            break;
         }

         /* Inline in the inode */
         case VMFS_BLK_TYPE_FD:
            if (blk_id == f->inode->id) {
               exp_len = m_min(len,file_size - pos);
               memcpy(buf, f->inode->content + pos, exp_len);
               res = exp_len;
               break;
            }

         default:
            fprintf(stderr,"VMFS: unknown block type 0x%2.2x\n",blk_type);
            return(-EIO);
      }

      /* Error while reading block, abort immediately */
      if (res < 0)
         return(res);

      /* Move file position and keep track of bytes currently read */
      pos += res;
      rlen += res;

      /* Move buffer position */
      buf += res;
      len -= res;
   }

   return(rlen);
}

/* Write data to a file at the specified position */
ssize_t vmfs_file_pwrite(vmfs_file_t *f,u_char *buf,size_t len,off_t pos)
{   
   const vmfs_fs_t *fs = vmfs_file_get_fs(f);
   uint32_t blk_id,blk_type;
   uint64_t blk_size;
   ssize_t res=0,wlen = 0;
   int err;

   if (!vmfs_fs_readwrite(fs))
      return(-EROFS);

   /* We don't handle RDM files */
   if (f->inode->type == VMFS_FILE_TYPE_RDM)
      return(-EIO);

   blk_size = vmfs_fs_get_blocksize(fs);

   while(len > 0) {
      if ((err = vmfs_inode_get_wrblock(f->inode,pos,&blk_id)) < 0)
         return(err);

#if 0
      if (f->vol->debug_level > 1)
         printf("vmfs_file_write: writing block 0x%8.8x\n",blk_id);
#endif

      blk_type = VMFS_BLK_TYPE(blk_id);

      switch(blk_type) {
         /* File-Block */
         case VMFS_BLK_TYPE_FB:
            res = vmfs_block_write_fb(fs,blk_id,pos,buf,len);
            break;

         /* Sub-Block */
         case VMFS_BLK_TYPE_SB:
            res = vmfs_block_write_sb(fs,blk_id,pos,buf,len);
            break;

         default:
            fprintf(stderr,"VMFS: unknown block type 0x%2.2x\n",blk_type);
            return(-EIO);
      }

      /* Error while writing block, abort immediately */
      if (res < 0)
         return(res);

      /* Move file position and keep track of bytes currently written */
      pos += res;
      wlen += res;

      /* Move buffer position */
      buf += res;
      len -= res;
   }

   /* Update file size */
   if (pos > vmfs_file_get_size(f)) {
      f->inode->size = pos;
      f->inode->update_flags |= VMFS_INODE_SYNC_META;
   }

   return(wlen);
}

/* Dump a file */
int vmfs_file_dump(vmfs_file_t *f,off_t pos,uint64_t len,FILE *fd_out)
{
   u_char *buf;
   ssize_t res;
   size_t clen,buf_len;

   if (!len)
      len = vmfs_file_get_size(f);

   buf_len = 0x100000;

   if (!(buf = iobuffer_alloc(buf_len)))
      return(-1);

   for(;pos < len; pos+=clen) {
      clen = m_min(len,buf_len);
      res = vmfs_file_pread(f,buf,clen,pos);

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
   }

   free(buf);
   return(0);
}

/* Get file status */
int vmfs_file_fstat(const vmfs_file_t *f,struct stat *buf)
{
   return(vmfs_inode_stat(f->inode,buf));
}

/* Get file file status (follow symlink) */
int vmfs_file_stat_at(vmfs_dir_t *dir,const char *path,struct stat *buf)
{
   uint32_t blk_id;

   if (!(blk_id = vmfs_dir_resolve_path(dir,path,1)))
      return(-ENOENT);

   return(vmfs_inode_stat_from_blkid(vmfs_dir_get_fs(dir),blk_id,buf));
}

/* Get file file status (do not follow symlink) */
int vmfs_file_lstat_at(vmfs_dir_t *dir,const char *path,struct stat *buf)
{
   const vmfs_dirent_t *entry;
   vmfs_dir_t *d;
   char *name;

   name = m_dirname(path);
   d = vmfs_dir_open_at(dir,name);
   free(name);
   if (!d)
      return(-1);

   name = m_basename(path);
   if (!strcmp(name,"/")) {
      free(name);
      return(vmfs_file_fstat(dir->dir,buf));
   }
   entry = vmfs_dir_lookup(dir,name);
   free(name);
   if (!entry)
      return(-1);

   return(vmfs_inode_stat_from_blkid(vmfs_dir_get_fs(dir),
                                     entry->block_id,buf));
}

/* Truncate a file (using a file descriptor) */
int vmfs_file_truncate(vmfs_file_t *f,off_t length)
{
   return(vmfs_inode_truncate(f->inode,length));
}

/* Truncate a file (using a path) */
int vmfs_file_truncate_at(vmfs_dir_t *dir,const char *path,off_t length)
{
   vmfs_file_t *f;
   int res;

   if (!(f = vmfs_file_open_at(dir,path)))
      return(-ENOENT);

   res = vmfs_file_truncate(f,length);

   vmfs_file_close(f);
   return(res);
}

/* Change permissions of a file */
int vmfs_file_chmod(vmfs_file_t *f,mode_t mode)
{
   return(vmfs_inode_chmod(f->inode,mode));
}

/* Change permissions of a file (using a path) */
int vmfs_file_chmod_at(vmfs_dir_t *dir,const char *path,mode_t mode)
{
   vmfs_file_t *f;
   int res;

   if (!(f = vmfs_file_open_at(dir,path)))
      return(-ENOENT);

   res = vmfs_file_chmod(f,mode);

   vmfs_file_close(f);
   return(res);
}

/* Delete a file */
int vmfs_file_delete(vmfs_dir_t *dir,const char *name)
{
   vmfs_dirent_t *entry;
   off_t pos;

   if (!(entry = (vmfs_dirent_t *)vmfs_dir_lookup(dir,name)))
      return(-ENOENT);

   if ((entry->type != VMFS_FILE_TYPE_FILE) &&
       (entry->type != VMFS_FILE_TYPE_SYMLINK))
      return(-EPERM);

   pos = (dir->pos - 1) * VMFS_DIRENT_SIZE;
   return(vmfs_dir_unlink_inode(dir,pos,entry));
}
