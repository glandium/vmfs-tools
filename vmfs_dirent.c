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
   memcpy(entry->name,buf+VMFS_DIRENT_OFS_NAME,sizeof(entry->name));
   entry->name[128] = 0;
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

   if (entry->type != VMFS_FILE_TYPE_SYMLINK)
      return NULL;

   if (!(f = vmfs_file_open_from_blkid(fs,entry->block_id)))
      return NULL;

   str_len = f->inode.size;

   if (!(str = malloc(str_len+1)))
      goto done;

   if (vmfs_file_read(f,(u_char *)str,str_len) != str_len)
      goto done;

   str[str_len] = 0;

 done:
   vmfs_file_close(f);
   return str;
}

/* Resolve a path name to a directory entry */
const vmfs_dirent_t *vmfs_dir_resolve_path(vmfs_dir_t *base_dir,
                                           const char *path,
                                           int follow_symlink)
{
   vmfs_dir_t *cur_dir,*sub_dir;
   const vmfs_dirent_t *rec;
   char *nam, *ptr,*sl,*symlink;
   int close_dir = 0;

   cur_dir = base_dir;

   if (!(rec = vmfs_dir_lookup(cur_dir,".")))
      return(NULL);

   nam = ptr = strdup(path);
   
   while(*ptr != 0) {
      sl = strchr(ptr,'/');

      if (sl != NULL)
         *sl = 0;

      if (*ptr == 0) {
         ptr = sl + 1;
         continue;
      }
             
      if (!(rec = vmfs_dir_lookup(cur_dir,ptr)))
         break;
      
      if ((sl == NULL) && !follow_symlink)
         break;

      /* follow the symlink if we have an entry of this type */
      if (rec->type == VMFS_FILE_TYPE_SYMLINK) {
         if (!(symlink = vmfs_dirent_read_symlink(base_dir->dir->fs,rec))) {
            rec = NULL;
            break;
         }

         rec = vmfs_dir_resolve_path(cur_dir,symlink,1);
         free(symlink);

         if (!rec)
            break;
      }

      /* last token */
      if (sl == NULL)
         break;

      /* we must have a directory here */
      if ((rec->type != VMFS_FILE_TYPE_DIR) ||
          !(sub_dir = vmfs_dir_open_from_blkid(base_dir->dir->fs,rec->block_id)))
      {
         rec = NULL;
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

   return(rec);
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
vmfs_dir_t *vmfs_dir_open_from_path(const vmfs_fs_t *fs,const char *path)
{
   return vmfs_dir_open_from_file(vmfs_file_open_from_path(fs,path));
}

/* Return next entry in directory. Returned directory entry will be overwritten
by subsequent calls */
const vmfs_dirent_t *vmfs_dir_read(vmfs_dir_t *d)
{
   u_char buf[VMFS_DIRENT_SIZE];

   if ((d == NULL) || (vmfs_file_pread(d->dir,buf,sizeof(buf),
                                       d->pos*sizeof(buf)) != sizeof(buf)))
      return(NULL);

   vmfs_dirent_read(&d->dirent,buf);
   d->pos++;

   return &d->dirent;
}

/* Close a directory */
int vmfs_dir_close(vmfs_dir_t *d)
{
   if (d == NULL)
      return(-1);

   vmfs_file_close(d->dir);
   free(d);
   return(0);
}
