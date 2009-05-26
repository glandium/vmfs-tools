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
enum {
   VMFS_BLK_TYPE_COW = 0,/* Copy-on-write for sparse files */
   VMFS_BLK_TYPE_FB,     /* File Block */
   VMFS_BLK_TYPE_SB,     /* Sub-Block */
   VMFS_BLK_TYPE_PB,     /* Pointer Block */
   VMFS_BLK_TYPE_FD,     /* File Descriptor */
   VMFS_BLK_TYPE_MAX,
};

/* Extract block type from a block ID */
#define VMFS_BLK_TYPE(blk_id)  ((blk_id) & 0x07)

/* File-Block - TBZ flag specifies if the block must be zeroed. */
#define VMFS_BLK_FB_ITEM(blk_id)   ((blk_id) >> 6)
#define VMFS_BLK_FB_TBZ(blk_id)    (((blk_id) >> 5) & 0x1)

/* Sub-Block */
#define VMFS_BLK_SB_ITEM(blk_id)   (((blk_id) >> 28) & 0x0f)
#define VMFS_BLK_SB_ENTRY(blk_id)  (((blk_id) & 0xfffffff) >> 6)

/* Pointer-Block */
#define VMFS_BLK_PB_ITEM(blk_id)   (((blk_id) >> 28) & 0x0f)
#define VMFS_BLK_PB_ENTRY(blk_id)  (((blk_id) & 0xfffffff) >> 6)

/* File Descriptor */
#define VMFS_BLK_FD_ITEM(blk_id)   (((blk_id) >> 22) & 0x3ff)
#define VMFS_BLK_FD_ENTRY(blk_id)  (((blk_id) >> 6)  & 0x7fff)

/* === VMFS block list === */
struct vmfs_blk_list {
   uint32_t total,last_pos;
   uint32_t *blk_id;
};

/* Initialize a block list */
int vmfs_blk_list_init(vmfs_blk_list_t *list,uint32_t blk_count);

/* Free a block list */
void vmfs_blk_list_free(vmfs_blk_list_t *list);

/* Set a block at the specified position */
int vmfs_blk_list_add_block(vmfs_blk_list_t *list,u_int pos,uint32_t blk_id);

/* Get a block ID from a block list, given its position */
int vmfs_blk_list_get_block(const vmfs_blk_list_t *list,u_int pos,
                            uint32_t *blk_id);

/* Show a block list */
void vmfs_blk_list_show(const vmfs_blk_list_t *list);

#endif
