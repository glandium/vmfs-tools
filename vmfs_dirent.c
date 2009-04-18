/* 
 * VMFS directory entries.
 */

#include "vmfs.h"

/* Read a file descriptor */
int vmfs_dirent_read(vmfs_dirent_t *entry,u_char *buf)
{
   entry->type      = read_le32(buf,VMFS_DIRENT_OFS_TYPE);
   entry->block_id  = read_le32(buf,VMFS_DIRENT_OFS_BLK_ID);
   entry->record_id = read_le32(buf,VMFS_DIRENT_OFS_REC_ID);
   memcpy(entry->name,buf+VMFS_DIRENT_OFS_NAME,sizeof(entry->name));
   return(0);
}

/* Search for an entry into a directory */
int vmfs_dirent_search(vmfs_file_t *dir_entry,char *name,vmfs_dirent_t *rec)
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

/* Resolve a path name to a directory entry */
int vmfs_dirent_resolve_path(vmfs_volume_t *vol,char *name,vmfs_dirent_t *rec)
{
   vmfs_file_t *cur_dir,*sub_dir;
   char *ptr,*sl;

   cur_dir = vol->root_dir;
   ptr = name;
   
   for(;;) {
      sl = strchr(ptr,'/');

      if (sl != NULL)
         *sl = 0;

      if (*ptr == 0) {
         ptr = sl + 1;
         continue;
      }
             
      if (vmfs_dirent_search(cur_dir,ptr,rec) != 1)
         return(-1);
      
      /* last token */
      if (sl == NULL)
         return(1);

      if (!(sub_dir = vmfs_file_open_rec(vol,rec)))
         return(-1);

#if 0 /* TODO */
      if (cur_dir != vol->root_dir)
         vmfs_file_close(cur_dir);
#endif
      cur_dir = sub_dir;
      ptr = sl + 1;
   }
}
