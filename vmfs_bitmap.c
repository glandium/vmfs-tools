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
/* 
 * VMFS bitmaps.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include "utils.h"
#include "vmfs.h"

/* Read a bitmap header */
int vmfs_bmh_read(vmfs_bitmap_header_t *bmh,const u_char *buf)
{
   bmh->items_per_bitmap_entry = read_le32(buf,0x0);
   bmh->bmp_entries_per_area   = read_le32(buf,0x4);
   bmh->hdr_size               = read_le32(buf,0x8);
   bmh->data_size              = read_le32(buf,0xc);
   bmh->area_size              = read_le32(buf,0x10);
   bmh->total_items            = read_le32(buf,0x14);
   bmh->area_count             = read_le32(buf,0x18);
   return(0);
}

/* Show bitmap information */
void vmfs_bmh_show(const vmfs_bitmap_header_t *bmh)
{
   printf("  - Items per bitmap entry: %d (0x%x)\n",
          bmh->items_per_bitmap_entry,bmh->items_per_bitmap_entry);

   printf("  - Bitmap entries per area: %d (0x%x)\n",
          bmh->bmp_entries_per_area,bmh->bmp_entries_per_area);

   printf("  - Header size: %d (0x%x)\n",bmh->hdr_size,bmh->hdr_size);
   printf("  - Data size: %d (0x%x)\n",bmh->data_size,bmh->data_size);
   printf("  - Area size: %d (0x%x)\n",bmh->area_size,bmh->area_size);
   printf("  - Area count: %d (0x%x)\n",bmh->area_count,bmh->area_count);
   printf("  - Total items: %d (0x%x)\n",bmh->total_items,bmh->total_items);
}

/* Read a bitmap entry */
int vmfs_bme_read(vmfs_bitmap_entry_t *bme,const u_char *buf,int copy_bitmap)
{
   bme->magic    = read_le32(buf,VMFS_BME_OFS_MAGIC);
   bme->pos      = read_le64(buf,VMFS_BME_OFS_POS);
   bme->hb_pos   = read_le64(buf,VMFS_BME_OFS_HB_POS);
   bme->hb_lock  = read_le32(buf,VMFS_BME_OFS_HB_LOCK);
   bme->id       = read_le32(buf,VMFS_BME_OFS_ID);
   bme->total    = read_le32(buf,VMFS_BME_OFS_TOTAL);
   bme->free     = read_le32(buf,VMFS_BME_OFS_FREE);
   bme->ffree    = read_le32(buf,VMFS_BME_OFS_FFREE);

   if (copy_bitmap) {
      memcpy(bme->bitmap,&buf[VMFS_BME_OFS_BITMAP],(bme->total+7)/8);
   }

   read_uuid(buf,VMFS_BME_OFS_HB_UUID,&bme->hb_uuid);
   return(0);
}

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

/* Get address of a block */
off_t vmfs_bitmap_get_block_addr(const vmfs_bitmap_header_t *bmh,uint32_t blk)
{
   uint32_t items_per_area;
   off_t addr;
   u_int area;

   items_per_area = vmfs_bitmap_get_items_per_area(bmh,blk);
   area = blk / items_per_area;

   addr  = vmfs_bitmap_get_area_data_addr(bmh,area);
   addr += (blk % items_per_area) * bmh->data_size;

   return(addr);
}

/* Read a bitmap entry given a block id */
int vmfs_bitmap_get_entry(vmfs_file_t *f,const vmfs_bitmap_header_t *bmh,
                          u_int blk,vmfs_bitmap_entry_t *entry)
{   
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   uint32_t items_per_area;
   u_int entry_idx,area;
   off_t addr;

   items_per_area = vmfs_bitmap_get_items_per_area(bmh,blk);
   area = blk / items_per_area;

   entry_idx = (blk % items_per_area) / bmh->items_per_bitmap_entry;

   addr = vmfs_bitmap_get_area_addr(bmh,area);
   addr += entry_idx * VMFS_BITMAP_ENTRY_SIZE;

   vmfs_file_seek(f,addr,SEEK_SET);

   if (vmfs_file_read(f,buf,VMFS_BITMAP_ENTRY_SIZE) != VMFS_BITMAP_ENTRY_SIZE)
      return(-1);

   vmfs_bme_read(entry,buf,1);
   return(0);
}

/* Read a bitmap item from its entry and item numbers */
bool vmfs_bitmap_get_item(vmfs_bitmap_t *b, uint32_t entry, uint32_t item,
                          u_char *buf)
{
   uint32_t addr = vmfs_bitmap_get_block_addr(&b->bmh,
                                 entry * b->bmh.items_per_bitmap_entry + item);
   vmfs_file_seek(b->f,addr,SEEK_SET);
   return (vmfs_file_read(b->f,buf,b->bmh.data_size) == b->bmh.data_size);
}


/* Get offset of an item in a bitmap entry */
static void 
vmfs_bitmap_get_item_offset(const vmfs_bitmap_header_t *bmh,u_int blk,
                            u_int *array_idx,u_int *bit_idx)
{
   u_int idx;

   idx = blk % bmh->items_per_bitmap_entry;
   *array_idx = idx / 8;
   *bit_idx   = idx & 0x07;
}

/* Mark an item as free or allocated */
int vmfs_bitmap_set_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *entry,
                                u_int blk,int status)
{
   u_int array_idx,bit_idx;
   u_int bit_mask;

   vmfs_bitmap_get_item_offset(bmh,blk,&array_idx,&bit_idx);
   bit_mask = 1 << bit_idx;

   if (status == 0) {
      /* item is already freed */
      if (entry->bitmap[array_idx] & bit_mask)
         return(-1);

      entry->bitmap[array_idx] |= bit_mask;
      entry->free++;
   } else {
      /* item is already allocated */
      if (!(entry->bitmap[array_idx] & bit_mask))
         return(-1);
      
      entry->bitmap[array_idx] &= ~bit_mask;
      entry->free--;
   }

   return(0);
}

/* Get the status of an item (0=free,1=allocated) */
int vmfs_bitmap_get_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *entry,u_int blk)
{
   u_int array_idx,bit_idx;
   u_int bit_mask;

   vmfs_bitmap_get_item_offset(bmh,blk,&array_idx,&bit_idx);
   bit_mask = 1 << bit_idx;

   return((entry->bitmap[array_idx] & bit_mask) ? 0 : 1);
}

/* Find a bitmap entry with at least "num_items" free in the specified area */
int vmfs_bitmap_area_find_free_items(vmfs_file_t *f,
                                     const vmfs_bitmap_header_t *bmh,
                                     u_int area,u_int num_items,
                                     vmfs_bitmap_entry_t *entry)
{
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   int i;

   vmfs_file_seek(f,vmfs_bitmap_get_area_addr(bmh,area),SEEK_SET);

   for(i=0;i<bmh->bmp_entries_per_area;i++) {
      if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
         break;

      vmfs_bme_read(entry,buf,1);

      if (entry->free >= num_items)
         return(0);
   }

   return(-1);
}

/* Find a bitmap entry with at least "num_items" free (scan all areas) */
int vmfs_bitmap_find_free_items(vmfs_file_t *f,
                                const vmfs_bitmap_header_t *bmh,
                                u_int area,u_int num_items,
                                vmfs_bitmap_entry_t *entry)
{
   u_int i;

   for(i=0;i<bmh->area_count;i++)
      if (!vmfs_bitmap_area_find_free_items(f,bmh,i,num_items,entry))
         return(0);

   return(-1);
}

/* Count the total number of allocated items in a bitmap area */
uint32_t vmfs_bitmap_area_allocated_items(vmfs_file_t *f,
                                          const vmfs_bitmap_header_t *bmh,
                                          u_int area)

{
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   uint32_t count;
   int i;

   vmfs_file_seek(f,vmfs_bitmap_get_area_addr(bmh,area),SEEK_SET);

   for(i=0,count=0;i<bmh->bmp_entries_per_area;i++) {
      if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
         break;

      vmfs_bme_read(&entry,buf,0);
      count += entry.total - entry.free;
   }

   return count;
}

/* Count the total number of allocated items in a bitmap */
uint32_t vmfs_bitmap_allocated_items(vmfs_file_t *f,
                                    const vmfs_bitmap_header_t *bmh)
{
   uint32_t count;
   u_int i;
   
   for(i=0,count=0;i<bmh->area_count;i++)
      count += vmfs_bitmap_area_allocated_items(f,bmh,i);

   return(count);
}

/* Check coherency of a bitmap file */
int vmfs_bitmap_check(vmfs_file_t *f,const vmfs_bitmap_header_t *bmh)
{  
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   uint32_t total_items;
   uint32_t magic;
   uint32_t entry_id;
   int i,j,k,errors;
   int bmap_size;
   int bmap_count;

   errors      = 0;
   total_items = 0;
   magic       = 0;
   entry_id    = 0;

   for(i=0;i<bmh->area_count;i++) {
      vmfs_file_seek(f,vmfs_bitmap_get_area_addr(bmh,i),SEEK_SET);

      for(j=0;j<bmh->bmp_entries_per_area;j++) {
         if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
            break;

         vmfs_bme_read(&entry,buf,0);

         if (entry.magic == 0)
            goto done;

         /* check the entry ID */
         if (entry.id != entry_id) {
            printf("Entry 0x%x has incorrect ID 0x%x\n",entry_id,entry.id);
            errors++;
         }
         
         /* check the magic number */
         if (magic == 0) {
            magic = entry.magic;
         } else {
            if (entry.magic != magic) {
               printf("Entry 0x%x has an incorrect magic id (0x%x)\n",
                      entry_id,entry.magic);
               errors++;
            }
         }
         
         /* check the number of items */
         if (entry.total > bmh->items_per_bitmap_entry) {
            printf("Entry 0x%x has an incorrect total of 0x%2.2x items\n",
                   entry_id,entry.total);
            errors++;
         }

         /* check the bitmap array */
         bmap_size = (entry.total + 7) / 8;
         bmap_count = 0;

         for(k=0;k<bmap_size;k++) {
            bmap_count += bit_count(buf[VMFS_BME_OFS_BITMAP+k]);
         }

         if (bmap_count != entry.free) {
            printf("Entry 0x%x has an incorrect bitmap array "
                   "(bmap_count=0x%x instead of 0x%x)\n",
                   entry_id,bmap_count,entry.free);
            errors++;
         }

         total_items += entry.total;
         entry_id++;
      }
   }

 done:
   if (total_items != bmh->total_items) {
      printf("Total number of items (0x%x) doesn't match header info (0x%x)\n",
             total_items,bmh->total_items);
      errors++;
   }

   return(errors);
}

/* Open a bitmap file */
static inline vmfs_bitmap_t *vmfs_bitmap_open_from_file(vmfs_file_t *f)
{
   DECL_ALIGNED_BUFFER(buf,512);
   vmfs_bitmap_t *b;

   if (!f)
      return NULL;

   if (vmfs_file_read(f,buf,buf_len) != buf_len) {
      vmfs_file_close(f);
      return NULL;
   }

   if (!(b = calloc(1, sizeof(vmfs_bitmap_t)))) {
      vmfs_file_close(f);
      return NULL;
   }

   vmfs_bmh_read(&b->bmh, buf);
   b->f = f;
   return b;
}

vmfs_bitmap_t *vmfs_bitmap_open_from_path(const vmfs_fs_t *fs,
                                          const char *path)
{
   return vmfs_bitmap_open_from_file(vmfs_file_open_from_path(fs, path));
}

vmfs_bitmap_t *vmfs_bitmap_open_from_inode(const vmfs_fs_t *fs,
                                           const u_char *inode_buf)
{
   return vmfs_bitmap_open_from_file(vmfs_file_open_from_inode(fs, inode_buf));
}


/* Close a bitmap file */
void vmfs_bitmap_close(vmfs_bitmap_t *b)
{
   vmfs_file_close(b->f);
   free(b);
}
