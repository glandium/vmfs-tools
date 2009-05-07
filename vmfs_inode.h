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
   uint32_t magic;
   uint64_t pos;
   uint64_t hb_pos;
   u_char _unknown0[16];
   uint32_t hb_lock;
   uuid_t hb_uuid;
   u_char _unknown1[456];
   uint32_t id;
   uint32_t id2;
   uint32_t _unknown2;
   uint32_t type;
   uint32_t _unknown3;
   uint64_t size;
   u_char _unknown4[16];
   uint32_t mtime;
   uint32_t ctime;
   uint32_t atime;
   uint32_t uid;
   uint32_t gid;
   uint32_t mode;
   u_char _unknown5[444];
   union {
      uint32_t blocks[VMFS_INODE_BLK_COUNT];
      uint32_t rdm_id;
   };
} __attribute__((packed));

#define VMFS_INODE_OFS_MAGIC      offsetof(struct vmfs_inode_raw, magic)
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
#define VMFS_INODE_OFS_RDM_ID     offsetof(struct vmfs_inode_raw, rdm_id)

struct vmfs_inode {
   uint32_t magic;
   uint64_t position;
   uint64_t hb_pos;
   uint32_t hb_lock;
   uuid_t  hb_uuid;
   uint32_t id,id2;
   uint32_t type;
   uint64_t size;
   time_t  mtime,ctime,atime;
   uint32_t uid,gid;
   uint32_t mode,cmode;
   uint32_t rdm_id;
};

/* Read an inode */
int vmfs_inode_read(vmfs_inode_t *inode,const u_char *buf);

/* Write an inode */
int vmfs_inode_write(const vmfs_inode_t *inode,u_char *buf);

/* Show an inode */
void vmfs_inode_show(const vmfs_inode_t *inode);

/* Get the offset corresponding to an inode in the FDC file */
off_t vmfs_inode_get_offset(const vmfs_fs_t *fs,uint32_t blk_id);

/* Get inode associated to a directory entry */
int vmfs_inode_get(const vmfs_fs_t *fs,const vmfs_dirent_t *rec,u_char *buf);

/* Bind inode info to a file */
int vmfs_inode_bind(vmfs_file_t *f,const u_char *inode_buf);

/* Get inode status */
int vmfs_inode_stat(const vmfs_inode_t *inode,struct stat *buf);

#endif
