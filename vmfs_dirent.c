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
 * VMFS directory entries.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include "vmfs.h"

/* Read a directory entry */
static int vmfs_dirent_read(vmfs_dirent_t *entry,const u_char *buf)
{
   entry->type      = read_le32(buf,VMFS_DIRENT_OFS_TYPE);
   entry->block_id  = read_le32(buf,VMFS_DIRENT_OFS_BLK_ID);
   entry->record_id = read_le32(buf,VMFS_DIRENT_OFS_REC_ID);
   memcpy(entry->name,buf+VMFS_DIRENT_OFS_NAME,VMFS_DIRENT_OFS_NAME_SIZE);
   entry->name[VMFS_DIRENT_OFS_NAME_SIZE] = 0;
   return(0);
}

/* Write a directory entry */
static int vmfs_dirent_write(const vmfs_dirent_t *entry,u_char *buf)
{
   write_le32(buf,VMFS_DIRENT_OFS_TYPE,entry->type);
   write_le32(buf,VMFS_DIRENT_OFS_BLK_ID,entry->block_id);
   write_le32(buf,VMFS_DIRENT_OFS_REC_ID,entry->record_id);
   memcpy(buf+VMFS_DIRENT_OFS_NAME,entry->name,VMFS_DIRENT_OFS_NAME_SIZE);
   return(0);
}

/* Show a directory entry */
void vmfs_dirent_show(const vmfs_dirent_t *entry)
{
   printf("  - Type      : 0x%x\n",entry->type);
   printf("  - Block ID  : 0x%8.8x\n",entry->block_id);
   printf("  - Record ID : 0x%8.8x\n",entry->record_id);
   printf("  - Name      : %s\n",entry->name);
}

/* Search for an entry into a directory ; affects position of the next
entry vmfs_dir_read will return */
const vmfs_dirent_t *vmfs_dir_lookup(vmfs_dir_t *d,const char *name)
{
   const vmfs_dirent_t *rec;
   vmfs_dir_seek(d,0);

   while((rec = vmfs_dir_read(d))) {
      if (!strcmp(rec->name,name))
         return(rec);
   }

   return(NULL);
}

/* Read a symlink */
static char *vmfs_dirent_read_symlink(const vmfs_fs_t *fs,
                                      const vmfs_dirent_t *entry)
{
   vmfs_file_t *f;
   size_t str_len;
   char *str = NULL;

   if (!(f = vmfs_file_open_from_blkid(fs,entry->block_id)))
      return NULL;

   str_len = vmfs_file_get_size(f);

   if (!(str = malloc(str_len+1)))
      goto done;

   if ((str_len = vmfs_file_pread(f,(u_char *)str,str_len,0)) == -1) {
      free(str);
      goto done;
   }

   str[str_len] = 0;

 done:
   vmfs_file_close(f);
   return str;
}

/* Resolve a path name to a block id */
uint32_t vmfs_dir_resolve_path(vmfs_dir_t *base_dir,const char *path,
                               int follow_symlink)
{
   vmfs_dir_t *cur_dir,*sub_dir;
   const vmfs_dirent_t *rec;
   char *nam, *ptr,*sl,*symlink;
   int close_dir = 0;
   const vmfs_fs_t *fs = vmfs_dir_get_fs(base_dir);
   uint32_t ret = 0;

   cur_dir = base_dir;

   if (*path == '/') {
      if (!(cur_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0))))
         return(0);
      path++;
      close_dir = 1;
   }

   if (!(rec = vmfs_dir_lookup(cur_dir,".")))
      return(0);

   ret = rec->block_id;

   nam = ptr = strdup(path);
   
   while(*ptr != 0) {
      sl = strchr(ptr,'/');

      if (sl != NULL)
         *sl = 0;

      if (*ptr == 0) {
         ptr = sl + 1;
         continue;
      }
             
      if (!(rec = vmfs_dir_lookup(cur_dir,ptr))) {
         ret = 0;
         break;
      }
      
      ret = rec->block_id;

      if ((sl == NULL) && !follow_symlink)
         break;

      /* follow the symlink if we have an entry of this type */
      if (rec->type == VMFS_FILE_TYPE_SYMLINK) {
         if (!(symlink = vmfs_dirent_read_symlink(fs,rec))) {
            ret = 0;
            break;
         }

         ret = vmfs_dir_resolve_path(cur_dir,symlink,1);
         free(symlink);

         if (!ret)
            break;
      }

      /* last token */
      if (sl == NULL)
         break;

      /* we must have a directory here */
      if (!(sub_dir = vmfs_dir_open_from_blkid(fs,ret)))
      {
         ret = 0;
         break;
      }

      if (close_dir)
         vmfs_dir_close(cur_dir);

      cur_dir = sub_dir;
      close_dir = 1;
      ptr = sl + 1;
   }
   free(nam);

   if (close_dir)
      vmfs_dir_close(cur_dir);

   return(ret);
}

/* Open a directory file using a callback */
static vmfs_dir_t *vmfs_dir_open_from_file(vmfs_file_t *file)
{
   vmfs_dir_t *d;

   if (file == NULL)
      return NULL;

   if (!(d = calloc(1, sizeof(*d))) ||
       (file->inode.type != VMFS_FILE_TYPE_DIR)) {
      vmfs_file_close(file);
      return NULL;
   }

   d->dir = file;
   d->buf = calloc(1,vmfs_file_get_size(file));
   if (d->buf)
      vmfs_file_pread(file,d->buf,vmfs_file_get_size(file),0);
   return d;
}

/* Open a directory based on an inode buffer */
vmfs_dir_t *vmfs_dir_open_from_inode(const vmfs_fs_t *fs,
                                    const vmfs_inode_t *inode)
{
   return vmfs_dir_open_from_file(vmfs_file_open_from_inode(fs,inode));
}

/* Open a directory based on a directory entry */
vmfs_dir_t *vmfs_dir_open_from_blkid(const vmfs_fs_t *fs,uint32_t blk_id)
{
   return vmfs_dir_open_from_file(vmfs_file_open_from_blkid(fs,blk_id));
}

/* Open a directory */
vmfs_dir_t *vmfs_dir_open_at(vmfs_dir_t *d,const char *path)
{
   return vmfs_dir_open_from_file(vmfs_file_open_at(d,path));
}

/* Return next entry in directory. Returned directory entry will be overwritten
by subsequent calls */
const vmfs_dirent_t *vmfs_dir_read(vmfs_dir_t *d)
{
   u_char *buf;
   if (d == NULL)
      return(NULL);

   if (d->buf) {
      if (d->pos*VMFS_DIRENT_SIZE >= vmfs_file_get_size(d->dir))
         return(NULL);
      buf = &d->buf[d->pos*VMFS_DIRENT_SIZE];
   } else {
      u_char _buf[VMFS_DIRENT_SIZE];
      if ((vmfs_file_pread(d->dir,_buf,sizeof(_buf),
                           d->pos*sizeof(_buf)) != sizeof(_buf)))
         return(NULL);
      buf = _buf;
   }

   vmfs_dirent_read(&d->dirent,buf);
   d->pos++;

   return &d->dirent;
}

/* Close a directory */
int vmfs_dir_close(vmfs_dir_t *d)
{
   if (d == NULL)
      return(-1);

   if (d->buf)
      free(d->buf);
   vmfs_file_close(d->dir);
   free(d);
   return(0);
}

/* Create a directory entry based on name and inode */
static int vmfs_dir_create_entry(vmfs_dir_t *d,vmfs_inode_t *inode,char *name)
{
   u_char buf[VMFS_DIRENT_SIZE];
   vmfs_dirent_t entry;
   off_t dir_size;

   memset(&entry,0,sizeof(entry));
   entry.type      = inode->type;
   entry.block_id  = inode->id;
   entry.record_id = inode->id2;
   strncpy(entry.name,name,VMFS_DIRENT_OFS_NAME_SIZE);
   vmfs_dirent_write(&entry,buf);

   dir_size = vmfs_file_get_size(d->dir);

   if (vmfs_file_pwrite(d->dir,buf,sizeof(buf),dir_size) != sizeof(buf))
      return(-1);

   inode->nlink++;
   return(0);
}

/* Create a new directory */
static int vmfs_dir_create_dir(vmfs_dir_t *d,char *name)
{
   vmfs_fs_t *fs = (vmfs_fs_t *)vmfs_dir_get_fs(d);
   vmfs_dir_t *new_dir;
   vmfs_inode_t *inode,tmp_inode;

   /* Allocate inode for the new directory */
   if (vmfs_inode_alloc(fs,&tmp_inode) == -1)
      return(-1);

   tmp_inode.type = VMFS_FILE_TYPE_DIR;

   if (!(new_dir = vmfs_dir_open_from_inode(fs,&tmp_inode)))
      goto err_open_dir;

   inode = &new_dir->dir->inode;

   vmfs_dir_create_entry(new_dir,inode,".");
   vmfs_dir_create_entry(new_dir,&d->dir->inode,"..");
   vmfs_dir_create_entry(d,inode,name);

   vmfs_inode_update(fs,inode,0);
   vmfs_inode_update(fs,&d->dir->inode,0);
   return(0);

 err_open_dir:
   vmfs_block_free(fs,tmp_inode.id);
   return(-1);
}
