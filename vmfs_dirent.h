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
#ifndef VMFS_DIRENT_H
#define VMFS_DIRENT_H

#include <stddef.h>

#define VMFS_DIRENT_SIZE    0x8c

struct vmfs_dirent_raw {
   uint32_t type;
   uint32_t block_id;
   uint32_t record_id;
   char name[128];
} __attribute__((packed));

#define VMFS_DIRENT_OFS_TYPE    offsetof(struct vmfs_dirent_raw, type)
#define VMFS_DIRENT_OFS_BLK_ID  offsetof(struct vmfs_dirent_raw, block_id)
#define VMFS_DIRENT_OFS_REC_ID  offsetof(struct vmfs_dirent_raw, record_id)
#define VMFS_DIRENT_OFS_NAME    offsetof(struct vmfs_dirent_raw, name)

#define VMFS_DIRENT_OFS_NAME_SIZE  sizeof(((struct vmfs_dirent_raw *)(0))->name)

struct vmfs_dirent {
   uint32_t type;
   uint32_t block_id;
   uint32_t record_id;
   char name[129];
};

/* Show a directory entry */
void vmfs_dirent_show(const vmfs_dirent_t *entry);

/* Search for an entry into a directory ; affects position of the next
entry vmfs_dir_read will return */
const vmfs_dirent_t *vmfs_dir_lookup(vmfs_dir_t *dir,const char *name);

/* Resolve a path to a directory entry */
const vmfs_dirent_t *vmfs_dir_resolve_path(vmfs_dir_t *base_dir,
                                           const char *path,
                                           int follow_symlink);

struct vmfs_dir {
   vmfs_file_t *dir;
   uint32_t pos;
   vmfs_dirent_t dirent;
};

/* Open a directory based on an inode buffer */
vmfs_dir_t *vmfs_dir_open_from_inode(const vmfs_fs_t *fs,
                                     const vmfs_inode_t *inode);

/* Open a directory based on a directory entry */
vmfs_dir_t *vmfs_dir_open_from_blkid(const vmfs_fs_t *fs,uint32_t blk_id);

/* Open a directory */
vmfs_dir_t *vmfs_dir_open_from_path(const vmfs_fs_t *fs,const char *path);

/* Return next entry in directory. Returned directory entry will be overwritten
by subsequent calls */
const vmfs_dirent_t *vmfs_dir_read(vmfs_dir_t *d);

/* Set position of the next entry that vmfs_dir_read will return */
static inline void vmfs_dir_seek(vmfs_dir_t *d, uint32_t pos)
{
   if (d)
      d->pos = pos;
}

/* Close a directory */
int vmfs_dir_close(vmfs_dir_t *d);

#endif
