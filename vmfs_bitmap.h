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
#define VMFS_BITMAP_ENTRY_SIZE  0x400

#define VMFS_BME_OFS_BITMAP     0x210

struct vmfs_bitmap_entry {
   uint32_t magic;
   uint64_t position;
   uint32_t id;
   uint32_t total;
   uint32_t free;
   uint32_t alloc;
   uint8_t bitmap[0];
};

/* Get number of items per area */
static inline u_int 
vmfs_bitmap_get_items_per_area(const vmfs_bitmap_header_t *bmh,u_int id)
{
   return(bmh->bmp_entries_per_area * bmh->items_per_bitmap_entry);
}

/* Get address of a given area (pointing to bitmap array) */
static inline off_t 
vmfs_bitmap_get_area_addr(const vmfs_bitmap_header_t *bmh,u_int area)
{
   return(bmh->hdr_size + (area * bmh->area_size));
}

/* Get data address of a given area (after the bitmap array) */
static inline off_t
vmfs_bitmap_get_area_data_addr(const vmfs_bitmap_header_t *bmh,u_int area)
{
   off_t addr;

   addr  = vmfs_bitmap_get_area_addr(bmh,area);
   addr += bmh->bmp_entries_per_area * VMFS_BITMAP_ENTRY_SIZE;
   return(addr);
}

/* Read a bitmap header */
int vmfs_bmh_read(vmfs_bitmap_header_t *bmh,const u_char *buf);

/* Show bitmap information */
void vmfs_bmh_show(const vmfs_bitmap_header_t *bmh);

/* Read a bitmap entry */
int vmfs_bme_read(vmfs_bitmap_entry_t *bme,const u_char *buf,int copy_bitmap);

/* Get address of a block */
off_t vmfs_bitmap_get_block_addr(const vmfs_bitmap_header_t *bmh,uint32_t blk);

/* Read a bitmap entry given a block id */
int vmfs_bitmap_get_entry(vmfs_file_t *f,const vmfs_bitmap_header_t *bmh,
                          u_int blk,vmfs_bitmap_entry_t *entry);

/* Mark an item as free or allocated */
int vmfs_bitmap_set_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *entry,
                                u_int blk,int status);

/* Get the status of an item (0=free,1=allocated) */
int vmfs_bitmap_get_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *entry,u_int blk);

/* Count the total number of allocated items in a bitmap area */
uint32_t vmfs_bitmap_area_allocated_items(vmfs_file_t *f,
                                         const vmfs_bitmap_header_t *bmh,
                                         u_int area);

/* Count the total number of allocated items in a bitmap */
uint32_t vmfs_bitmap_allocated_items(vmfs_file_t *f,
                                    const vmfs_bitmap_header_t *bmh);

/* Check coherency of a bitmap file */
int vmfs_bitmap_check(vmfs_file_t *f,const vmfs_bitmap_header_t *bmh);

#endif
