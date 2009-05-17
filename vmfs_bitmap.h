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
#ifndef VMFS_BITMAP_H
#define VMFS_BITMAP_H

#include <stdbool.h>

/* === Bitmap header === */
struct vmfs_bitmap_header {
   uint32_t items_per_bitmap_entry;
   uint32_t bmp_entries_per_area;
   uint32_t hdr_size;
   uint32_t data_size;
   uint32_t area_size;
   uint32_t total_items;
   uint32_t area_count;
};

/* === Bitmap entry === */
#define VMFS_BITMAP_ENTRY_SIZE    0x400

#define VMFS_BITMAP_BMP_MAX_SIZE  0x1f0

struct vmfs_bitmap_entry_raw {
   uint32_t magic;            /* Magic number */
   uint64_t pos;              /* Position in the volume */
   uint64_t hb_pos;           /* Heartbeat position */
   uint64_t hb_seq;           /* Heartbeat sequence */
   uint64_t obj_seq;          /* Object sequence */
   uint32_t hb_lock;          /* Heartbeat lock flag */
   uuid_t hb_uuid;            /* UUID of locking server */
   u_char _unknown1[0x1c8];
   uint32_t id;               /* Bitmap ID */
   uint32_t total;            /* Total number of items in this entry */
   uint32_t free;             /* Free items */
   uint32_t ffree;            /* First free item */
   uint8_t bitmap[VMFS_BITMAP_BMP_MAX_SIZE];
} __attribute__((packed));

#define VMFS_BME_OFS_MAGIC    offsetof(struct vmfs_bitmap_entry_raw, magic)
#define VMFS_BME_OFS_POS      offsetof(struct vmfs_bitmap_entry_raw, pos)
#define VMFS_BME_OFS_HB_POS   offsetof(struct vmfs_bitmap_entry_raw, hb_pos)
#define VMFS_BME_OFS_HB_SEQ   offsetof(struct vmfs_bitmap_entry_raw, hb_seq)
#define VMFS_BME_OFS_OBJ_SEQ  offsetof(struct vmfs_bitmap_entry_raw, obj_seq)
#define VMFS_BME_OFS_HB_LOCK  offsetof(struct vmfs_bitmap_entry_raw, hb_lock)
#define VMFS_BME_OFS_HB_UUID  offsetof(struct vmfs_bitmap_entry_raw, hb_uuid)
#define VMFS_BME_OFS_ID       offsetof(struct vmfs_bitmap_entry_raw, id)
#define VMFS_BME_OFS_TOTAL    offsetof(struct vmfs_bitmap_entry_raw, total)
#define VMFS_BME_OFS_FREE     offsetof(struct vmfs_bitmap_entry_raw, free)
#define VMFS_BME_OFS_FFREE    offsetof(struct vmfs_bitmap_entry_raw, ffree)
#define VMFS_BME_OFS_BITMAP   offsetof(struct vmfs_bitmap_entry_raw, bitmap)

struct vmfs_bitmap_entry {
   uint32_t magic;
   uint64_t pos;
   uint64_t hb_pos;
   uint64_t hb_seq;
   uint64_t obj_seq;
   uint32_t hb_lock;
   uuid_t hb_uuid;
   uint32_t id;
   uint32_t total;
   uint32_t free;
   uint32_t ffree;
   uint8_t bitmap[0];
};

/* A bitmap file instance */
struct vmfs_bitmap {
   vmfs_file_t *f;
   vmfs_bitmap_header_t bmh;
};

/* Show bitmap information */
void vmfs_bmh_show(const vmfs_bitmap_header_t *bmh);

/* Read a bitmap entry */
int vmfs_bme_read(vmfs_bitmap_entry_t *bme,const u_char *buf,int copy_bitmap);

/* Read a bitmap entry given a block id */
int vmfs_bitmap_get_entry(vmfs_bitmap_t *b,u_int blk,
                          vmfs_bitmap_entry_t *entry);

/* Read a bitmap item from its entry and item numbers */
bool vmfs_bitmap_get_item(vmfs_bitmap_t *b, uint32_t entry, uint32_t item,
                          u_char *buf);

/* Mark an item as free or allocated */
int vmfs_bitmap_set_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *entry,
                                u_int blk,int status);

/* Get the status of an item (0=free,1=allocated) */
int vmfs_bitmap_get_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *entry,u_int blk);

/* Find a bitmap entry with at least "num_items" free in the specified area */
int vmfs_bitmap_area_find_free_items(vmfs_bitmap_t *b,
                                     u_int area,u_int num_items,
                                     vmfs_bitmap_entry_t *entry);

/* Find a bitmap entry with at least "num_items" free (scan all areas) */
int vmfs_bitmap_find_free_items(vmfs_bitmap_t *b,u_int num_items,
                                vmfs_bitmap_entry_t *entry);

/* Count the total number of allocated items in a bitmap area */
uint32_t vmfs_bitmap_area_allocated_items(vmfs_bitmap_t *b,u_int area);

/* Count the total number of allocated items in a bitmap */
uint32_t vmfs_bitmap_allocated_items(vmfs_bitmap_t *b);

/* Check coherency of a bitmap file */
int vmfs_bitmap_check(vmfs_bitmap_t *b);

/* Open a bitmap file */
vmfs_bitmap_t *vmfs_bitmap_open_from_path(const vmfs_fs_t *fs,
                                          const char *path);

vmfs_bitmap_t *vmfs_bitmap_open_from_inode(const vmfs_fs_t *fs,
                                           const u_char *inode_buf);

/* Close a bitmap file */
void vmfs_bitmap_close(vmfs_bitmap_t *b);

#endif
