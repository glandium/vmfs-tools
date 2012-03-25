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
#ifndef VMFS_BLOCK_H
#define VMFS_BLOCK_H

/* Block types */
enum vmfs_block_type {
   VMFS_BLK_TYPE_NONE = 0,
   VMFS_BLK_TYPE_FB,     /* File Block */
   VMFS_BLK_TYPE_SB,     /* Sub-Block */
   VMFS_BLK_TYPE_PB,     /* Pointer Block */
   VMFS_BLK_TYPE_FD,     /* File Descriptor */
   VMFS_BLK_TYPE_MAX,
};

#define VMFS_BLK_SHIFT(mask) __builtin_ctz(mask)
#define VMFS_BLK_VALUE(blk_id, mask) (((blk_id) & (mask)) >> VMFS_BLK_SHIFT(mask))
#define VMFS_BLK_MAX_VALUE(mask) (((mask) >> VMFS_BLK_SHIFT(mask)) + 1)
#define VMFS_BLK_FILL(value, mask) (((value) << VMFS_BLK_SHIFT(mask)) & (mask))

#define VMFS_BLK_TYPE_MASK  0x00000007

/* Extract block type from a block ID */
#define VMFS_BLK_TYPE(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_TYPE_MASK)

/* File-Block
 * { unsigned int item:26;
 *   unsigned int flags:3;
 *   unsigned int type:3; }
 * There is probably really no more than one flag, but so far, nothing
 * indicates what can be stored between the significant bits for the block
 * type and the TBZ flag, so we'll consider they are flags of some sort,
 * and will display them as such.
 */
#define VMFS_BLK_FB_ITEM_MASK  0xffffffc0
#define VMFS_BLK_FB_FLAGS_MASK 0x00000038

/* TBZ flag specifies if the block must be zeroed. */
#define VMFS_BLK_FB_TBZ_FLAG    4

#define VMFS_BLK_FB_ITEM(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_FB_ITEM_MASK)
#define VMFS_BLK_FB_FLAGS(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_FB_FLAGS_MASK)

#define VMFS_BLK_FB_MAX_ITEM VMFS_BLK_MAX_VALUE(VMFS_BLK_FB_ITEM_MASK)

#define VMFS_BLK_FB_TBZ(blk_id) \
   (VMFS_BLK_FB_FLAGS(blk_id) & VMFS_BLK_FB_TBZ_FLAG)

#define VMFS_BLK_FB_TBZ_CLEAR(blk_id) ((blk_id) & ~(VMFS_BLK_FILL(VMFS_BLK_FB_TBZ_FLAG, VMFS_BLK_FB_FLAGS_MASK)))

#define VMFS_BLK_FB_BUILD(item, flags) \
   (VMFS_BLK_FILL(item, VMFS_BLK_FB_ITEM_MASK) | \
    VMFS_BLK_FILL(flags, VMFS_BLK_FB_FLAGS_MASK) | \
    VMFS_BLK_TYPE_FB)

/* Sub-Block
 * { unsigned int item_lsb:4;
 *   unsigned int entry:22;
 *   unsigned int flags:1; // Not sure it even exists
 *   unsigned int item_msb: 2;
 *   unsigned int type:3; }
 */
#define VMFS_BLK_SB_ITEM_LSB_MASK 0xf0000000
#define VMFS_BLK_SB_ENTRY_MASK    0x0fffffc0
#define VMFS_BLK_SB_FLAGS_MASK    0x00000020
#define VMFS_BLK_SB_ITEM_MSB_MASK 0x00000018

#define VMFS_BLK_SB_ITEM_VALUE_LSB_MASK 0x0000000f
#define VMFS_BLK_SB_ITEM_VALUE_MSB_MASK 0x00000030

#define VMFS_BLK_SB_ITEM(blk_id) \
   (VMFS_BLK_FILL(VMFS_BLK_VALUE(blk_id, VMFS_BLK_SB_ITEM_LSB_MASK), VMFS_BLK_SB_ITEM_VALUE_LSB_MASK) | \
    VMFS_BLK_FILL(VMFS_BLK_VALUE(blk_id, VMFS_BLK_SB_ITEM_MSB_MASK), VMFS_BLK_SB_ITEM_VALUE_MSB_MASK))
#define VMFS_BLK_SB_ENTRY(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_SB_ENTRY_MASK)
#define VMFS_BLK_SB_FLAGS(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_SB_FLAGS_MASK)

#define VMFS_BLK_SB_MAX_ITEM VMFS_BLK_MAX_VALUE(VMFS_BLK_SB_ITEM_VALUE_LSB_MASK | VMFS_BLK_SB_ITEM_VALUE_MSB_MASK)
#define VMFS_BLK_SB_MAX_ENTRY VMFS_BLK_MAX_VALUE(VMFS_BLK_SB_ENTRY_MASK)

#define VMFS_BLK_SB_BUILD(entry, item, flags) \
   (VMFS_BLK_FILL(entry, VMFS_BLK_SB_ENTRY_MASK) | \
    VMFS_BLK_FILL(VMFS_BLK_VALUE(item, VMFS_BLK_SB_ITEM_VALUE_LSB_MASK), \
                  VMFS_BLK_SB_ITEM_LSB_MASK) | \
    VMFS_BLK_FILL(VMFS_BLK_VALUE(item, VMFS_BLK_SB_ITEM_VALUE_MSB_MASK), \
                  VMFS_BLK_SB_ITEM_MSB_MASK) | \
    VMFS_BLK_FILL(flags, VMFS_BLK_SB_FLAGS_MASK) | \
    VMFS_BLK_TYPE_SB)

/* Pointer-Block
 * { unsigned int item:4;
 *   unsigned int entry:22;
 *   unsigned int flags:3;
 *   unsigned int type:3; }
 */
#define VMFS_BLK_PB_ITEM_MASK  0xf0000000
#define VMFS_BLK_PB_ENTRY_MASK 0x0fffffc0
#define VMFS_BLK_PB_FLAGS_MASK 0x00000038

#define VMFS_BLK_PB_ITEM(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_PB_ITEM_MASK)
#define VMFS_BLK_PB_ENTRY(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_PB_ENTRY_MASK)
#define VMFS_BLK_PB_FLAGS(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_PB_FLAGS_MASK)

#define VMFS_BLK_PB_MAX_ITEM VMFS_BLK_MAX_VALUE(VMFS_BLK_PB_ITEM_MASK)
#define VMFS_BLK_PB_MAX_ENTRY VMFS_BLK_MAX_VALUE(VMFS_BLK_PB_ENTRY_MASK)

#define VMFS_BLK_PB_BUILD(entry, item, flags) \
   (VMFS_BLK_FILL(entry, VMFS_BLK_PB_ENTRY_MASK) | \
    VMFS_BLK_FILL(item, VMFS_BLK_PB_ITEM_MASK) | \
    VMFS_BLK_FILL(flags, VMFS_BLK_PB_FLAGS_MASK) | \
    VMFS_BLK_TYPE_PB)

/* File Descriptor
 * { unsigned int item:10;
 *   unsigned int entry:16;
 *   unsigned int flags:3;
 *   unsigned int type:3; }
 */
#define VMFS_BLK_FD_ITEM_MASK  0xffc00000
#define VMFS_BLK_FD_ENTRY_MASK 0x003fffc0
#define VMFS_BLK_FD_FLAGS_MASK 0x00000038

#define VMFS_BLK_FD_ITEM(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_FD_ITEM_MASK)
#define VMFS_BLK_FD_ENTRY(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_FD_ENTRY_MASK)
#define VMFS_BLK_FD_FLAGS(blk_id) VMFS_BLK_VALUE(blk_id, VMFS_BLK_FD_FLAGS_MASK)

#define VMFS_BLK_FD_MAX_ITEM VMFS_BLK_MAX_VALUE(VMFS_BLK_FD_ITEM_MASK)
#define VMFS_BLK_FD_MAX_ENTRY VMFS_BLK_MAX_VALUE(VMFS_BLK_FD_ENTRY_MASK)

#define VMFS_BLK_FD_BUILD(entry, item, flags) \
   (VMFS_BLK_FILL(entry, VMFS_BLK_FD_ENTRY_MASK) | \
    VMFS_BLK_FILL(item, VMFS_BLK_FD_ITEM_MASK) | \
    VMFS_BLK_FILL(flags, VMFS_BLK_FD_FLAGS_MASK) | \
    VMFS_BLK_TYPE_FD)

struct vmfs_block_info {
   uint32_t entry, item, flags;
   enum vmfs_block_type type;
};

/* Get bitmap info (bitmap type,entry and item) from a block ID */
int vmfs_block_get_info(uint32_t blk_id, vmfs_block_info_t *info);

/* Get block status (allocated or free) */
int vmfs_block_get_status(const vmfs_fs_t *fs,uint32_t blk_id);

/* Allocate the specified block */
int vmfs_block_alloc_specified(const vmfs_fs_t *fs,uint32_t blk_id);

/* Free the specified block */
int vmfs_block_free(const vmfs_fs_t *fs,uint32_t blk_id);

/* Allocate a single block */
int vmfs_block_alloc(const vmfs_fs_t *fs,uint32_t blk_type,uint32_t *blk_id);

/* Zeroize a file block */
int vmfs_block_zeroize_fb(const vmfs_fs_t *fs,uint32_t blk_id);

/* Free blocks hold by a pointer block */
int vmfs_block_free_pb(const vmfs_fs_t *fs,uint32_t pb_blk,                     
                       u_int start,u_int end);

/* Read a piece of a sub-block */
ssize_t vmfs_block_read_sb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                           u_char *buf,size_t len);

/* Write a piece of a sub-block */
ssize_t vmfs_block_write_sb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                            u_char *buf,size_t len);

/* Read a piece of a file block */
ssize_t vmfs_block_read_fb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                           u_char *buf,size_t len);

/* Write a piece of a file block */
ssize_t vmfs_block_write_fb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                            u_char *buf,size_t len);

#endif
