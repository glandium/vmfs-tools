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
int vmfs_dirent_read(vmfs_dirent_t *entry,const u_char *buf)
{
   entry->type      = read_le32(buf,VMFS_DIRENT_OFS_TYPE);
   entry->block_id  = read_le32(buf,VMFS_DIRENT_OFS_BLK_ID);
   entry->record_id = read_le32(buf,VMFS_DIRENT_OFS_REC_ID);
   entry->name = strndup((char *)buf+VMFS_DIRENT_OFS_NAME,
                         VMFS_DIRENT_OFS_NAME_SIZE);
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

/* Search for an entry into a directory */
int vmfs_dirent_search(vmfs_file_t *dir_entry,const char *name,vmfs_dirent_t *rec)
{
   u_char buf[VMFS_DIRENT_SIZE];
   int dir_count;
   ssize_t len;

   dir_count = vmfs_file_get_size(dir_entry) / VMFS_DIRENT_SIZE;
   vmfs_file_seek(dir_entry,0,SEEK_SET);
   
   while(dir_count > 0) {
      len = vmfs_file_read(dir_entry,buf,sizeof(buf));

      if (len != VMFS_DIRENT_SIZE)
         return(-1);

      vmfs_dirent_read(rec,buf);

      if (!strcmp(rec->name,name))
         return(1);
   }

   return(0);
}

/* Read a symlink */
static char *vmfs_dirent_read_symlink(const vmfs_fs_t *fs,vmfs_dirent_t *entry)
{
   vmfs_file_t *f;
   size_t str_len;
   char *str = NULL;

   if (entry->type != VMFS_FILE_TYPE_SYMLINK)
      return NULL;

   if (!(f = vmfs_file_open_rec(fs,entry)))
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
int vmfs_dirent_resolve_path(const vmfs_fs_t *fs,vmfs_file_t *base_dir,
                             const char *name,int follow_symlink,
                             vmfs_dirent_t *rec)
{
   vmfs_file_t *cur_dir,*sub_dir;
   char *nam, *ptr,*sl,*symlink;
   int close_dir = 0;
   int res = 1;
   int res2;

   cur_dir = base_dir;

   if (vmfs_dirent_search(cur_dir,".",rec) != 1)
      return(-1);

   nam = ptr = strdup(name);
   
   while(*ptr != 0) {
      sl = strchr(ptr,'/');

      if (sl != NULL)
         *sl = 0;

      if (*ptr == 0) {
         ptr = sl + 1;
         continue;
      }
             
      if (vmfs_dirent_search(cur_dir,ptr,rec) != 1) {
         res = -1;
         break;
      }
      
      if ((sl == NULL) && !follow_symlink) {
         res = 1;
         break;
      }

      /* follow the symlink if we have an entry of this type */
      if (rec->type == VMFS_FILE_TYPE_SYMLINK) {
         if (!(symlink = vmfs_dirent_read_symlink(fs,rec))) {
            res = -1;
            break;
         }

         res2 = vmfs_dirent_resolve_path(fs,cur_dir,symlink,1,rec);
         free(symlink);

         if (res2 != 1)
            break;
      }

      /* last token */
      if (sl == NULL) {
         res = 1;
         break;
      }

      /* we must have a directory here */
      if ((rec->type != VMFS_FILE_TYPE_DIR) ||
          !(sub_dir = vmfs_file_open_rec(fs,rec)))
      {
         res = -1;
         break;
      }

      if (close_dir)
         vmfs_file_close(cur_dir);

      cur_dir = sub_dir;
      close_dir = 1;
      ptr = sl + 1;
   }
   free(nam);

   if (close_dir)
      vmfs_file_close(cur_dir);

   return(res);
}

/* Free a directory list (returned by readdir) */
void vmfs_dirent_free_dlist(int count,vmfs_dirent_t ***dlist)
{
   int i;

   if (*dlist != NULL) {
      for(i=0;i<count;i++)
         free((*dlist)[i]);

      free(*dlist);
      *dlist = NULL;
   }
}

/* Read a directory */
int vmfs_dirent_readdir(const vmfs_fs_t *fs,const char *dir,vmfs_dirent_t ***dlist)
{
   u_char buf[VMFS_DIRENT_SIZE];
   vmfs_dirent_t *entry;
   vmfs_file_t *f;
   int i,dcount = 0;

   *dlist = NULL;
   dcount = 0;

   if (!(f = vmfs_file_open(fs,dir)))
      return(-1);
   
   dcount = f->inode.size / VMFS_DIRENT_SIZE;
   *dlist = calloc(dcount,sizeof(vmfs_dirent_t *));

   if (*dlist == NULL) {
      vmfs_file_close(f);
      return(-1);
   }

   for(i=0;i<dcount;i++) {
      if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
         goto error;

      if (!(entry = malloc(sizeof(*entry))))
         goto error;
      
      vmfs_dirent_read(entry,buf);
      (*dlist)[i] = entry;
   }

   vmfs_file_close(f);
   return(dcount);

 error:
   vmfs_dirent_free_dlist(dcount,dlist);
   vmfs_file_close(f);
   return(-1);
}
