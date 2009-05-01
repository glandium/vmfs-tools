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
#ifndef VMFS_INODE_H
#define VMFS_INODE_H

#include <stddef.h>
#include <sys/stat.h>

#define VMFS_INODE_SIZE  0x800
#define VMFS_INODE_BLK_COUNT      0x100

struct vmfs_inode_raw {
   m_u32_t group_id;
   m_u64_t pos;
   m_u64_t hb_pos;
   u_char _unknown0[16];
   m_u32_t hb_lock;
   uuid_t hb_uuid;
   u_char _unknown1[456];
   m_u32_t id;
   m_u32_t id2;
   m_u32_t _unknown2;
   m_u32_t type;
   m_u32_t _unknown3;
   m_u64_t size;
   u_char _unknown4[16];
   m_u32_t mtime;
   m_u32_t ctime;
   m_u32_t atime;
   m_u32_t uid;
   m_u32_t gid;
   m_u32_t mode;
   u_char _unknown5[444];
   m_u32_t blocks[VMFS_INODE_BLK_COUNT];
} __attribute__((packed));

#define VMFS_INODE_OFS_GRP_ID     offsetof(struct vmfs_inode_raw, group_id)
#define VMFS_INODE_OFS_POS        offsetof(struct vmfs_inode_raw, pos)
#define VMFS_INODE_OFS_HB_POS     offsetof(struct vmfs_inode_raw, hb_pos)
#define VMFS_INODE_OFS_HB_LOCK    offsetof(struct vmfs_inode_raw, hb_lock)
#define VMFS_INODE_OFS_HB_UUID    offsetof(struct vmfs_inode_raw, hb_uuid)
#define VMFS_INODE_OFS_ID         offsetof(struct vmfs_inode_raw, id)
#define VMFS_INODE_OFS_ID2        offsetof(struct vmfs_inode_raw, id2)
#define VMFS_INODE_OFS_TYPE       offsetof(struct vmfs_inode_raw, type)
#define VMFS_INODE_OFS_SIZE       offsetof(struct vmfs_inode_raw, size)
#define VMFS_INODE_OFS_MTIME      offsetof(struct vmfs_inode_raw, mtime)
#define VMFS_INODE_OFS_CTIME      offsetof(struct vmfs_inode_raw, ctime)
#define VMFS_INODE_OFS_ATIME      offsetof(struct vmfs_inode_raw, atime)
#define VMFS_INODE_OFS_UID        offsetof(struct vmfs_inode_raw, uid)
#define VMFS_INODE_OFS_GID        offsetof(struct vmfs_inode_raw, gid)
#define VMFS_INODE_OFS_MODE       offsetof(struct vmfs_inode_raw, mode)

#define VMFS_INODE_OFS_BLK_ARRAY  offsetof(struct vmfs_inode_raw, blocks)

struct vmfs_inode {
   m_u32_t group_id;
   m_u64_t position;
   m_u64_t hb_pos;
   m_u32_t hb_lock;
   uuid_t  hb_uuid;
   m_u32_t id,id2;
   m_u32_t type;
   m_u64_t size;
   time_t  mtime,ctime,atime;
   m_u32_t uid,gid;
   m_u32_t mode,cmode;
};

/* Read an inode */
int vmfs_inode_read(vmfs_inode_t *inode,const u_char *buf);

/* Write an inode */
int vmfs_inode_write(const vmfs_inode_t *inode,u_char *buf);

/* Show an inode */
void vmfs_inode_show(const vmfs_inode_t *inode);

/* Get the offset corresponding to an inode in the FDC file */
off_t vmfs_inode_get_offset(const vmfs_fs_t *fs,m_u32_t blk_id);

/* Get inode associated to a directory entry */
int vmfs_inode_get(const vmfs_fs_t *fs,const vmfs_dirent_t *rec,u_char *buf);

/* Bind inode info to a file */
int vmfs_inode_bind(vmfs_file_t *f,const u_char *inode_buf);

/* Get inode status */
int vmfs_inode_stat(const vmfs_inode_t *inode,struct stat *buf);

#endif
