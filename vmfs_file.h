#ifndef VMFS_FILE_H
#define VMFS_FILE_H

/* === VMFS file abstraction === */
struct vmfs_file {
   vmfs_volume_t *vol;
   vmfs_blk_list_t blk_list;
   vmfs_inode_t inode;

   /* Current position in file */
   off_t pos;

   /* ... */
};

/* Get file size */
static inline m_u64_t vmfs_file_get_size(vmfs_file_t *f)
{
   return(f->inode.size);
}

/* Create a file structure */
vmfs_file_t *vmfs_file_create_struct(vmfs_volume_t *vol);

/* Open a file based on a directory entry */
vmfs_file_t *vmfs_file_open_rec(vmfs_volume_t *vol,vmfs_dirent_t *rec);

/* Open a file */
vmfs_file_t *vmfs_file_open(vmfs_volume_t *vol,char *filename);

/* Set position */
int vmfs_file_seek(vmfs_file_t *f,off_t pos,int whence);

/* Read data from a file */
ssize_t vmfs_file_read(vmfs_file_t *f,u_char *buf,size_t len);

#endif
