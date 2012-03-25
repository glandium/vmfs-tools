/*
 * vmfs-tools - Tools to access VMFS filesystems
 * Copyright (C) 2009 Christophe Fillot <cf@utc.fr>
 * Copyright (C) 2009,2011 Mike Hommey <mh@glandium.org>
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

#define VMFS_INODE_MAGIC  0x10c00001

struct vmfs_inode_raw {
   struct vmfs_metadata_hdr_raw mdh;
   uint32_t id;
   uint32_t id2;              /* seems to be VMFS_BLK_FD_ITEM(id) + 1 */
   uint32_t nlink;
   uint32_t type;
   uint32_t flags;
   uint64_t size;
   uint64_t blk_size;
   uint64_t blk_count;
   uint32_t mtime;
   uint32_t ctime;
   uint32_t atime;
   uint32_t uid;
   uint32_t gid;
   uint32_t mode;
   uint32_t zla;
   uint32_t tbz;
   uint32_t cow;
   u_char _unknown2[432];
   union {
      uint32_t blocks[VMFS_INODE_BLK_COUNT];
      uint32_t rdm_id;
      char content[VMFS_INODE_BLK_COUNT * sizeof(uint32_t)];
   };
} __attribute__((packed));

#define VMFS_INODE_OFS_ID         offsetof(struct vmfs_inode_raw, id)
#define VMFS_INODE_OFS_ID2        offsetof(struct vmfs_inode_raw, id2)
#define VMFS_INODE_OFS_NLINK      offsetof(struct vmfs_inode_raw, nlink)
#define VMFS_INODE_OFS_TYPE       offsetof(struct vmfs_inode_raw, type)
#define VMFS_INODE_OFS_FLAGS      offsetof(struct vmfs_inode_raw, flags)
#define VMFS_INODE_OFS_SIZE       offsetof(struct vmfs_inode_raw, size)
#define VMFS_INODE_OFS_BLK_SIZE   offsetof(struct vmfs_inode_raw, blk_size)
#define VMFS_INODE_OFS_BLK_COUNT  offsetof(struct vmfs_inode_raw, blk_count)
#define VMFS_INODE_OFS_MTIME      offsetof(struct vmfs_inode_raw, mtime)
#define VMFS_INODE_OFS_CTIME      offsetof(struct vmfs_inode_raw, ctime)
#define VMFS_INODE_OFS_ATIME      offsetof(struct vmfs_inode_raw, atime)
#define VMFS_INODE_OFS_UID        offsetof(struct vmfs_inode_raw, uid)
#define VMFS_INODE_OFS_GID        offsetof(struct vmfs_inode_raw, gid)
#define VMFS_INODE_OFS_MODE       offsetof(struct vmfs_inode_raw, mode)
#define VMFS_INODE_OFS_ZLA        offsetof(struct vmfs_inode_raw, zla)
#define VMFS_INODE_OFS_TBZ        offsetof(struct vmfs_inode_raw, tbz)
#define VMFS_INODE_OFS_COW        offsetof(struct vmfs_inode_raw, cow)

#define VMFS_INODE_OFS_BLK_ARRAY  offsetof(struct vmfs_inode_raw, blocks)
#define VMFS_INODE_OFS_RDM_ID     offsetof(struct vmfs_inode_raw, rdm_id)
#define VMFS_INODE_OFS_CONTENT    offsetof(struct vmfs_inode_raw, content)

/* Synchronization flags */
#define VMFS_INODE_SYNC_META  0x01
#define VMFS_INODE_SYNC_BLK   0x02
#define VMFS_INODE_SYNC_ALL   (VMFS_INODE_SYNC_META | VMFS_INODE_SYNC_BLK)

/* Some VMFS 5 features use a weird ZLA */
#define VMFS5_ZLA_BASE 4301

struct vmfs_inode {
   vmfs_metadata_hdr_t mdh;
   uint32_t id,id2;
   uint32_t nlink;
   uint32_t type;
   uint32_t flags;
   uint64_t size;
   uint64_t blk_size;
   uint64_t blk_count;
   time_t  mtime,ctime,atime;
   uint32_t uid,gid;
   uint32_t mode,cmode;
   uint32_t zla,tbz,cow;
   uint32_t rdm_id;
   union {
     uint32_t blocks[VMFS_INODE_BLK_COUNT];
     char content[VMFS_INODE_BLK_COUNT * sizeof(uint32_t)];
   };

   /* In-core inode information */
   const vmfs_fs_t *fs;
   vmfs_inode_t **pprev,*next;
   u_int ref_count;
   u_int update_flags;
};

/* Callback function for vmfs_inode_foreach_block() */
typedef void (*vmfs_inode_foreach_block_cbk_t)(const vmfs_inode_t *inode,
                                               uint32_t pb_blk,
                                               uint32_t blk_id,
                                               void *opt_arg);

/* Update an inode on disk */
int vmfs_inode_update(const vmfs_inode_t *inode,int update_blk_list);

/* Get inode corresponding to a block id */
int vmfs_inode_get(const vmfs_fs_t *fs,uint32_t blk_id,vmfs_inode_t *inode);

/* Acquire an inode */
vmfs_inode_t *vmfs_inode_acquire(const vmfs_fs_t *fs,uint32_t blk_id);

/* Release an inode */
void vmfs_inode_release(vmfs_inode_t *inode);

/* Allocate a new inode */
int vmfs_inode_alloc(vmfs_fs_t *fs,u_int type,mode_t mode,vmfs_inode_t **inode);

/* 
 * Get block ID corresponding the specified position. Pointer block
 * resolution is transparently done here.
 */
int vmfs_inode_get_block(const vmfs_inode_t *inode,off_t pos,uint32_t *blk_id);

/* Get a block for writing corresponding to the specified position */
int vmfs_inode_get_wrblock(vmfs_inode_t *inode,off_t pos,uint32_t *blk_id);

/* Truncate file */
int vmfs_inode_truncate(vmfs_inode_t *inode,off_t new_len);

/* Call a function for each allocated block of an inode */
int vmfs_inode_foreach_block(const vmfs_inode_t *inode,
                             vmfs_inode_foreach_block_cbk_t cbk,void *opt_arg);

/* Get inode status */
int vmfs_inode_stat(const vmfs_inode_t *inode,struct stat *buf);

/* Get inode status */
int vmfs_inode_stat_from_blkid(const vmfs_fs_t *fs,uint32_t blk_id,
                               struct stat *buf);

/* Change permissions */
int vmfs_inode_chmod(vmfs_inode_t *inode,mode_t mode);

#endif
