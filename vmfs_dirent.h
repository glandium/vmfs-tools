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
int vmfs_dirent_read(vmfs_dirent_t *entry,const u_char *buf);

/* Show a directory entry */
void vmfs_dirent_show(const vmfs_dirent_t *entry);

/* Search for an entry into a directory */
int vmfs_dirent_search(vmfs_file_t *dir_entry,const char *name,
                       vmfs_dirent_t *rec);

/* Resolve a path name to a directory entry */
int vmfs_dirent_resolve_path(const vmfs_fs_t *fs, vmfs_file_t *base_dir,
                             const char *name,int follow_symlink,
                             vmfs_dirent_t *rec);

/* Free a directory list (returned by readdir) */
void vmfs_dirent_free_dlist(int count,vmfs_dirent_t ***dlist);

/* Read a directory */
int vmfs_dirent_readdir(const vmfs_fs_t *fs,const char *dir,vmfs_dirent_t ***dlist);

#endif
