/* 
 * VMFS directory entries.
 */

#include "vmfs.h"

/* Read a directory entry */
int vmfs_dirent_read(vmfs_dirent_t *entry,u_char *buf)
{
   entry->type      = read_le32(buf,VMFS_DIRENT_OFS_TYPE);
   entry->block_id  = read_le32(buf,VMFS_DIRENT_OFS_BLK_ID);
   entry->record_id = read_le32(buf,VMFS_DIRENT_OFS_REC_ID);
   memcpy(entry->name,buf+VMFS_DIRENT_OFS_NAME,sizeof(entry->name));
   return(0);
}

/* Show a directory entry */
void vmfs_dirent_show(vmfs_dirent_t *entry)
{
   printf("  - Type      : 0x%x\n",entry->type);
   printf("  - Block ID  : 0x%8.8x\n",entry->block_id);
   printf("  - Record ID : 0x%8.8x\n",entry->record_id);
   printf("  - Name      : %s\n",entry->name);
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

/* Read a symlink */
static char *vmfs_dirent_read_symlink(vmfs_volume_t *vol,vmfs_dirent_t *entry)
{
   vmfs_file_t *f;
   size_t str_len;
   char *str;

   if (entry->type != VMFS_FILE_TYPE_SYMLINK)
      return NULL;

   if (!(f = vmfs_file_open_rec(vol,entry)))
      return NULL;

   str_len = f->inode.size;

   if (!(str = malloc(str_len+1)))
      return NULL;

   if (vmfs_file_read(f,(u_char *)str,str_len) != str_len)
      return NULL;

   str[str_len] = 0;

   //vmfs_close_file(f);
   return str;
}

/* Resolve a path name to a directory entry */
int vmfs_dirent_resolve_path(vmfs_volume_t *vol,vmfs_file_t *base_dir,
                             char *name,vmfs_dirent_t *rec)
{
   vmfs_file_t *cur_dir,*sub_dir;
   char *ptr,*sl,*symlink;

   cur_dir = base_dir;

   if (vmfs_dirent_search(cur_dir,".",rec) != 1)
      return(-1);

   ptr = name;
   
   while(*ptr != 0) {
      sl = strchr(ptr,'/');

      if (sl != NULL)
         *sl = 0;

      if (*ptr == 0) {
         ptr = sl + 1;
         continue;
      }
             
      if (vmfs_dirent_search(cur_dir,ptr,rec) != 1)
         return(-1);
      
      /* follow the symlink if we have an entry of this type */
      if (rec->type == VMFS_FILE_TYPE_SYMLINK) {
         if (!(symlink = vmfs_dirent_read_symlink(vol,rec)))
            return(-1);

         if (vmfs_dirent_resolve_path(vol,cur_dir,symlink,rec) != 1)
            return(-1);
         
         free(symlink);
      }

      /* last token */
      if (sl == NULL)
         return(1);

      /* we must have a directory here */
      if (rec->type != VMFS_FILE_TYPE_DIR)
         return(-1);

      if (!(sub_dir = vmfs_file_open_rec(vol,rec)))
         return(-1);

#if 0 /* TODO */
      if (cur_dir != vol->root_dir)
         vmfs_file_close(cur_dir);
#endif
      cur_dir = sub_dir;
      ptr = sl + 1;
   }

   return(1);
}
