#ifndef VMFS_DIRENT_H
#define VMFS_DIRENT_H

#define VMFS_DIRENT_SIZE    0x8c

#define VMFS_DIRENT_OFS_TYPE    0x0000
#define VMFS_DIRENT_OFS_BLK_ID  0x0004
#define VMFS_DIRENT_OFS_REC_ID  0x0008
#define VMFS_DIRENT_OFS_NAME    0x000c

struct vmfs_dirent {
   m_u32_t type;
   m_u32_t block_id;
   m_u32_t record_id;
   char name[128];
};

/* Read a file descriptor */
int vmfs_dirent_read(vmfs_dirent_t *entry,u_char *buf);

/* Show a directory entry */
void vmfs_dirent_show(vmfs_dirent_t *entry);

/* Search for an entry into a directory */
int vmfs_dirent_search(vmfs_file_t *dir_entry,char *name,vmfs_dirent_t *rec);

/* Resolve a path name to a directory entry */
int vmfs_dirent_resolve_path(vmfs_volume_t *vol,vmfs_file_t *base_dir,
                             char *name,vmfs_dirent_t *rec);

/* Free a directory list (returned by readdir) */
void vmfs_dirent_free_dlist(int count,vmfs_dirent_t ***dlist);

/* Read a directory */
int vmfs_dirent_readdir(vmfs_volume_t *vol,char *dir,vmfs_dirent_t ***dlist);

#endif
