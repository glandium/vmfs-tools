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
#ifndef VMFS_FS_H
#define VMFS_FS_H

#include <stddef.h>

/* === FS Info === */
#define VMFS_FSINFO_BASE   0x0200000
#define VMFS_FSINFO_MAGIC  0x2fabf15e

struct vmfs_fsinfo_raw {
   uint32_t magic;
   uint32_t volver;
   u_char ver;
   uuid_t uuid;
   uint32_t mode;
   char label[128];
   uint32_t dev_blocksize;
   uint64_t blocksize;
   uint32_t ctime; /* ctime? in seconds */
   uint32_t _unknown3;
   uuid_t lvm_uuid;
   u_char _unknown4[16];
   uint32_t fdc_header_size;
   uint32_t fdc_bitmap_count;
   uint32_t subblock_size;
} __attribute__((packed));

#define VMFS_FSINFO_OFS_MAGIC    offsetof(struct vmfs_fsinfo_raw, magic)
#define VMFS_FSINFO_OFS_VOLVER   offsetof(struct vmfs_fsinfo_raw, volver)
#define VMFS_FSINFO_OFS_VER      offsetof(struct vmfs_fsinfo_raw, ver)
#define VMFS_FSINFO_OFS_UUID     offsetof(struct vmfs_fsinfo_raw, uuid)
#define VMFS_FSINFO_OFS_MODE     offsetof(struct vmfs_fsinfo_raw, mode)
#define VMFS_FSINFO_OFS_LABEL    offsetof(struct vmfs_fsinfo_raw, label)
#define VMFS_FSINFO_OFS_BLKSIZE  offsetof(struct vmfs_fsinfo_raw, blocksize)
#define VMFS_FSINFO_OFS_CTIME    offsetof(struct vmfs_fsinfo_raw, ctime)
#define VMFS_FSINFO_OFS_LVM_UUID offsetof(struct vmfs_fsinfo_raw, lvm_uuid)
#define VMFS_FSINFO_OFS_SBSIZE   offsetof(struct vmfs_fsinfo_raw, subblock_size)

#define VMFS_FSINFO_OFS_FDC_HEADER_SIZE \
   offsetof(struct vmfs_fsinfo_raw, fdc_header_size)

#define VMFS_FSINFO_OFS_FDC_BITMAP_COUNT \
   offsetof(struct vmfs_fsinfo_raw, fdc_bitmap_count)

#define VMFS_FSINFO_OFS_LABEL_SIZE sizeof(((struct vmfs_fsinfo_raw *)(0))->label)

struct vmfs_fsinfo {
   uint32_t magic;
   uint32_t vol_version;
   uint32_t version;
   uint32_t mode;
   uuid_t uuid;
   char *label;
   time_t ctime;

   uint64_t block_size;
   uint32_t subblock_size;

   uint32_t fdc_header_size;
   uint32_t fdc_bitmap_count;

   uuid_t lvm_uuid;
};

/* === VMFS filesystem === */
#define VMFS_INODE_HASH_BUCKETS  256

struct vmfs_fs {
   int debug_level;

   /* FS information */
   vmfs_fsinfo_t fs_info;

   /* Associated VMFS Device */
   vmfs_device_t *dev;

   /* Meta-files containing file system structures */
   vmfs_bitmap_t *fbb,*sbc,*pbc,*fdc;

   /* Heartbeat used to lock meta-data */
   vmfs_heartbeat_t hb;
   u_int hb_id;
   uint64_t hb_seq;
   u_int hb_refcount;
   uint64_t hb_expire;

   /* Counter for "gen" field in inodes */
   uint32_t inode_gen;

   /* In-core inodes hash table */
   u_int inode_hash_buckets;
   vmfs_inode_t **inodes;
};

/* Get the bitmap corresponding to the given type */
static inline vmfs_bitmap_t *vmfs_fs_get_bitmap(const vmfs_fs_t *fs,
                                                enum vmfs_block_type type)
{
   if ((type > VMFS_BLK_TYPE_NONE) && (type < VMFS_BLK_TYPE_MAX)) {
      vmfs_bitmap_t * const *bitmap = (vmfs_bitmap_t **)&fs->fbb;
      return bitmap[type - 1];
   }
   return NULL;
}

/* Get block size of a volume */
static inline uint64_t vmfs_fs_get_blocksize(const vmfs_fs_t *fs)
{
   return(fs->fs_info.block_size);
}

/* Get read-write status of a FS */
static inline bool vmfs_fs_readwrite(const vmfs_fs_t *fs)
{
   return(fs->dev->write);
}

/* Read a block from the filesystem */
ssize_t vmfs_fs_read(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                     u_char *buf,size_t len);

/* Write a block to the filesystem */
ssize_t vmfs_fs_write(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                      const u_char *buf,size_t len);

/* Open a FS */
vmfs_fs_t *vmfs_fs_open(char **paths, vmfs_flags_t flags);

/* Close a FS */
void vmfs_fs_close(vmfs_fs_t *fs);

#endif
