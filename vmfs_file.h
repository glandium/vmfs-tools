#ifndef VMFS_FILE_H
#define VMFS_FILE_H

#include <sys/stat.h>

/* File types (in inode and directory entries) */
#define VMFS_FILE_TYPE_DIR      0x02
#define VMFS_FILE_TYPE_FILE     0x03
#define VMFS_FILE_TYPE_SYMLINK  0x04

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

/* Close a file */
int vmfs_file_close(vmfs_file_t *f);

/* Set position */
int vmfs_file_seek(vmfs_file_t *f,off_t pos,int whence);

/* Read data from a file */
ssize_t vmfs_file_read(vmfs_file_t *f,u_char *buf,size_t len);

/* Dump a file */
int vmfs_file_dump(vmfs_file_t *f,off_t pos,size_t len,FILE *fd_out);

/* Get file status */
int vmfs_file_fstat(vmfs_file_t *f,struct stat *buf);

/* Get file status (similar to fstat(), but with a path) */
int vmfs_file_stat(vmfs_volume_t *vol,char *path,struct stat *buf);

#endif
