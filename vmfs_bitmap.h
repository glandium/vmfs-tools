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
   m_u32_t items_per_bitmap_entry;
   m_u32_t bmp_entries_per_area;
   m_u32_t hdr_size;
   m_u32_t data_size;
   m_u32_t area_size;
   m_u32_t total_items;
   m_u32_t area_count;
};

/* === Bitmap entry === */
#define VMFS_BITMAP_ENTRY_SIZE  0x400

#define VMFS_BME_OFS_BITMAP     0x210

struct vmfs_bitmap_entry {
   m_u32_t magic;
   m_u64_t position;
   m_u32_t id;
   m_u32_t total;
   m_u32_t free;
   m_u32_t alloc;
   m_u8_t bitmap[0];
};

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
off_t vmfs_bitmap_get_block_addr(const vmfs_bitmap_header_t *bmh,m_u32_t blk);

/* Count the total number of allocated items in a bitmap area */
m_u32_t vmfs_bitmap_area_allocated_items(vmfs_file_t *f,
                                         const vmfs_bitmap_header_t *bmh,
                                         u_int area);

/* Count the total number of allocated items in a bitmap */
m_u32_t vmfs_bitmap_allocated_items(vmfs_file_t *f,
                                    const vmfs_bitmap_header_t *bmh);

/* Check coherency of a bitmap file */
int vmfs_bitmap_check(vmfs_file_t *f,const vmfs_bitmap_header_t *bmh);

#endif
