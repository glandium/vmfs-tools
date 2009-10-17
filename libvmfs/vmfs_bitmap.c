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

/* Write a bitmap header */
int vmfs_bmh_write(const vmfs_bitmap_header_t *bmh,u_char *buf)
{
   write_le32(buf,0x0,bmh->items_per_bitmap_entry);
   write_le32(buf,0x4,bmh->bmp_entries_per_area);
   write_le32(buf,0x8,bmh->hdr_size);
   write_le32(buf,0xc,bmh->data_size);
   write_le32(buf,0x10,bmh->area_size);
   write_le32(buf,0x14,bmh->total_items);
   write_le32(buf,0x18,bmh->area_count);
   return(0);
}

/* Show bitmap header information */
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
   vmfs_metadata_hdr_read(&bme->mdh,buf);
   bme->id     = read_le32(buf,VMFS_BME_OFS_ID);
   bme->total  = read_le32(buf,VMFS_BME_OFS_TOTAL);
   bme->free   = read_le32(buf,VMFS_BME_OFS_FREE);
   bme->ffree  = read_le32(buf,VMFS_BME_OFS_FFREE);

   if (copy_bitmap) {
      memcpy(bme->bitmap,&buf[VMFS_BME_OFS_BITMAP],(bme->total+7)/8);
   }

   return(0);
}

/* Write a bitmap entry */
int vmfs_bme_write(const vmfs_bitmap_entry_t *bme,u_char *buf)
{
   vmfs_metadata_hdr_write(&bme->mdh,buf);
   write_le32(buf,VMFS_BME_OFS_ID,bme->id);
   write_le32(buf,VMFS_BME_OFS_TOTAL,bme->total);
   write_le32(buf,VMFS_BME_OFS_FREE,bme->free);
   write_le32(buf,VMFS_BME_OFS_FFREE,bme->ffree);
   memcpy(&buf[VMFS_BME_OFS_BITMAP],bme->bitmap,(bme->total+7)/8);
   return(0);
}

/* Update a bitmap entry on disk */
int vmfs_bme_update(const vmfs_fs_t *fs,const vmfs_bitmap_entry_t *bme)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_BITMAP_ENTRY_SIZE);

   memset(buf,0,VMFS_BITMAP_ENTRY_SIZE);
   vmfs_bme_write(bme,buf);

   if (vmfs_device_write(&fs->lvm->dev,bme->mdh.pos,buf,buf_len) != buf_len)
      return(-1);

   return(0);
}

/* Show bitmap entry information */
void vmfs_bme_show(const vmfs_bitmap_entry_t *bme)
{
   vmfs_metadata_hdr_show(&bme->mdh);

   printf("  - ID           : 0x%8.8x\n",bme->id);
   printf("  - Total        : %u\n",bme->total);
   printf("  - Free         : %u\n",bme->free);
   printf("  - First free   : %u\n",bme->ffree);
}

/* Get number of items per area */
static inline u_int
vmfs_bitmap_get_items_per_area(const vmfs_bitmap_header_t *bmh)
{
   return(bmh->bmp_entries_per_area * bmh->items_per_bitmap_entry);
}

/* Get address of a given area (pointing to bitmap array) */
static inline off_t
vmfs_bitmap_get_area_addr(const vmfs_bitmap_header_t *bmh,u_int area)
{
   return(bmh->hdr_size + (area * bmh->area_size));
}

/* Read a bitmap entry given a block id */
int vmfs_bitmap_get_entry(vmfs_bitmap_t *b,uint32_t entry,uint32_t item,
                          vmfs_bitmap_entry_t *bmp_entry)
{   
   DECL_ALIGNED_BUFFER(buf,VMFS_BITMAP_ENTRY_SIZE);
   uint32_t items_per_area;
   u_int entry_idx,area;
   off_t addr;

   addr = (entry * b->bmh.items_per_bitmap_entry) + item;

   items_per_area = vmfs_bitmap_get_items_per_area(&b->bmh);
   area = addr / items_per_area;

   entry_idx = (addr % items_per_area) / b->bmh.items_per_bitmap_entry;

   addr = vmfs_bitmap_get_area_addr(&b->bmh,area);
   addr += entry_idx * VMFS_BITMAP_ENTRY_SIZE;

   if (vmfs_file_pread(b->f,buf,buf_len,addr) != buf_len)
      return(-1);

   vmfs_bme_read(bmp_entry,buf,1);
   return(0);
}

/* Get position of an item */
off_t vmfs_bitmap_get_item_pos(vmfs_bitmap_t *b,uint32_t entry,uint32_t item)
{
   off_t pos;
   uint32_t addr;
   uint32_t items_per_area;
   u_int area;

   addr = (entry * b->bmh.items_per_bitmap_entry) + item;

   items_per_area = vmfs_bitmap_get_items_per_area(&b->bmh);
   area = addr / items_per_area;

   pos  = b->bmh.hdr_size + (area * b->bmh.area_size);
   pos += b->bmh.bmp_entries_per_area * VMFS_BITMAP_ENTRY_SIZE;
   pos += (addr % items_per_area) * b->bmh.data_size;

   return(pos);
}

/* Read a bitmap item from its entry and item numbers */
bool vmfs_bitmap_get_item(vmfs_bitmap_t *b, uint32_t entry, uint32_t item,
                          u_char *buf)
{
   off_t pos = vmfs_bitmap_get_item_pos(b,entry,item);
   return(vmfs_file_pread(b->f,buf,b->bmh.data_size,pos) == b->bmh.data_size);
}

/* Write a bitmap given its entry and item numbers */
bool vmfs_bitmap_set_item(vmfs_bitmap_t *b,uint32_t entry,uint32_t item,
                          u_char *buf)
{
   off_t pos = vmfs_bitmap_get_item_pos(b,entry,item);
   return(vmfs_file_pwrite(b->f,buf,b->bmh.data_size,pos) == b->bmh.data_size);
}

/* Get offset of an item in a bitmap entry */
static void 
vmfs_bitmap_get_item_offset(const vmfs_bitmap_header_t *bmh,u_int addr,
                            u_int *array_idx,u_int *bit_idx)
{
   u_int idx;

   idx = addr % bmh->items_per_bitmap_entry;
   *array_idx = idx >> 3;
   *bit_idx   = idx & 0x07;
}

/* Update the first free item field */
static void vmfs_bitmap_update_ffree(vmfs_bitmap_entry_t *entry)
{
   u_int array_idx,bit_idx;
   int i;

   entry->ffree = 0;

   for(i=0;i<entry->total;i++) {
      array_idx = i >> 3;
      bit_idx   = i & 0x07;

      if (entry->bitmap[array_idx] & (1 << bit_idx)) {
         entry->ffree = i;
         break;
      }
   }
}

/* Mark an item as free or allocated */
int vmfs_bitmap_set_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *bmp_entry,
                                uint32_t entry,uint32_t item,
                                int status)
{
   u_int array_idx,bit_idx;
   u_int bit_mask;
   uint32_t addr;

   addr = (entry * bmh->items_per_bitmap_entry) + item;

   vmfs_bitmap_get_item_offset(bmh,addr,&array_idx,&bit_idx);
   bit_mask = 1 << bit_idx;

   if (status == 0) {
      /* item is already freed */
      if (bmp_entry->bitmap[array_idx] & bit_mask)
         return(-1);

      bmp_entry->bitmap[array_idx] |= bit_mask;
      bmp_entry->free++;
   } else {
      /* item is already allocated */
      if (!(bmp_entry->bitmap[array_idx] & bit_mask))
         return(-1);
      
      bmp_entry->bitmap[array_idx] &= ~bit_mask;
      bmp_entry->free--;
   }

   vmfs_bitmap_update_ffree(bmp_entry);
   return(0);
}

/* Get the status of an item (0=free,1=allocated) */
int vmfs_bitmap_get_item_status(const vmfs_bitmap_header_t *bmh,
                                vmfs_bitmap_entry_t *bmp_entry,
                                uint32_t entry,uint32_t item)
{
   u_int array_idx,bit_idx;
   u_int bit_mask;
   uint32_t addr;

   addr = (entry * bmh->items_per_bitmap_entry) + item;

   vmfs_bitmap_get_item_offset(bmh,addr,&array_idx,&bit_idx);
   bit_mask = 1 << bit_idx;

   return((bmp_entry->bitmap[array_idx] & bit_mask) ? 0 : 1);
}

/* Find a free item in a bitmap entry and mark it allocated */
int vmfs_bitmap_alloc_item(vmfs_bitmap_entry_t *bmp_entry,uint32_t *item)
{
   u_int array_idx,bit_idx;
   int i;

   /* TODO: use first free field as a hint */

   for(i=0;i<bmp_entry->total;i++) {
      array_idx = i >> 3;
      bit_idx   = i & 0x07;

      if (bmp_entry->bitmap[array_idx] & (1 << bit_idx)) {
         *item = i;
         bmp_entry->bitmap[array_idx] &= ~(1 << bit_idx);
         bmp_entry->free--;
         vmfs_bitmap_update_ffree(bmp_entry);
         return(0);
      }
   }

   return(-1);
}

/* Find a bitmap entry with at least "num_items" free in the specified area */
int vmfs_bitmap_area_find_free_items(vmfs_bitmap_t *b,
                                     u_int area,u_int num_items,
                                     vmfs_bitmap_entry_t *entry)
{
   vmfs_fs_t *fs;
   u_char *buf,*ptr;
   size_t buf_len;
   off_t pos;
   int res = -1;
   int i;

   fs = (vmfs_fs_t *)vmfs_file_get_fs(b->f);
   pos = vmfs_bitmap_get_area_addr(&b->bmh,area);
   buf_len = b->bmh.bmp_entries_per_area * VMFS_BITMAP_ENTRY_SIZE;

   if (!(buf = iobuffer_alloc(buf_len)))
      return(-1);

   if (vmfs_file_pread(b->f,buf,buf_len,pos) != buf_len)
      goto done;

   for(i=0;i<b->bmh.bmp_entries_per_area;i++) {
      ptr = buf + (i * VMFS_BITMAP_ENTRY_SIZE);
      vmfs_bme_read(entry,ptr,1);

      if (vmfs_metadata_is_locked(&entry->mdh) || (entry->free < num_items))
         continue;

      /* We now have to re-read the bitmap entry with the reservation taken */
      if (!vmfs_metadata_lock(fs,entry->mdh.pos,
                              ptr,VMFS_BITMAP_ENTRY_SIZE,
                              &entry->mdh))
      {      
         vmfs_bme_read(entry,ptr,1);

         if (entry->free < num_items) {
            vmfs_metadata_unlock(fs,&entry->mdh);
            continue;
         }

         res = 0;
         break;
      }
   }

 done:
   iobuffer_free(buf);
   return(res);
}

/* Find a bitmap entry with at least "num_items" free (scan all areas) */
int vmfs_bitmap_find_free_items(vmfs_bitmap_t *b,u_int num_items,
                                vmfs_bitmap_entry_t *entry)
{
   u_int i;

   for(i=0;i<b->bmh.area_count;i++)
      if (!vmfs_bitmap_area_find_free_items(b,i,num_items,entry))
         return(0);

   return(-1);
}

/* Count the total number of allocated items in a bitmap area */
uint32_t vmfs_bitmap_area_allocated_items(vmfs_bitmap_t *b,u_int area)
{
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   uint32_t count;
   off_t pos;
   int i;

   pos = vmfs_bitmap_get_area_addr(&b->bmh,area);

   for(i=0,count=0;i<b->bmh.bmp_entries_per_area;i++) {
      if (vmfs_file_pread(b->f,buf,sizeof(buf),pos) != sizeof(buf))
         break;

      vmfs_bme_read(&entry,buf,0);
      count += entry.total - entry.free;
      pos += sizeof(buf);
   }

   return count;
}

/* Count the total number of allocated items in a bitmap */
uint32_t vmfs_bitmap_allocated_items(vmfs_bitmap_t *b)
{
   uint32_t count;
   u_int i;
   
   for(i=0,count=0;i<b->bmh.area_count;i++)
      count += vmfs_bitmap_area_allocated_items(b,i);

   return(count);
}

/* Call a user function for each allocated item in a bitmap */
void vmfs_bitmap_area_foreach(vmfs_bitmap_t *b,u_int area,
                              vmfs_bitmap_foreach_cbk_t cbk,
                              void *opt_arg)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_BITMAP_ENTRY_SIZE);
   vmfs_bitmap_entry_t entry;
   off_t pos;
   uint32_t addr;
   u_int array_idx,bit_idx;
   u_int i,j;

   pos = vmfs_bitmap_get_area_addr(&b->bmh,area);

   for(i=0;i<b->bmh.bmp_entries_per_area;i++) {
      if (vmfs_file_pread(b->f,buf,buf_len,pos) != buf_len)
         break;

      vmfs_bme_read(&entry,buf,1);

      for(j=0;j<entry.total;j++) {
         array_idx = j >> 3;
         bit_idx   = j & 0x07;
         
         addr =  area * vmfs_bitmap_get_items_per_area(&b->bmh);
         addr += i * b->bmh.items_per_bitmap_entry;
         addr += j;

         if (!(entry.bitmap[array_idx] & (1 << bit_idx)))
            cbk(b,addr,opt_arg);
      }

      pos += buf_len;
   }
}

/* Call a user function for each allocated item in a bitmap */
void vmfs_bitmap_foreach(vmfs_bitmap_t *b,vmfs_bitmap_foreach_cbk_t cbk,
                         void *opt_arg)
{
   u_int i;
   
   for(i=0;i<b->bmh.area_count;i++)
      vmfs_bitmap_area_foreach(b,i,cbk,opt_arg);
}

/* Check coherency of a bitmap file */
int vmfs_bitmap_check(vmfs_bitmap_t *b)
{  
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   uint32_t total_items;
   uint32_t magic;
   uint32_t entry_id;
   int i,j,k,errors;
   int bmap_size;
   int bmap_count;
   off_t pos;

   errors      = 0;
   total_items = 0;
   magic       = 0;
   entry_id    = 0;

   for(i=0;i<b->bmh.area_count;i++) {
      pos = vmfs_bitmap_get_area_addr(&b->bmh,i);

      for(j=0;j<b->bmh.bmp_entries_per_area;j++) {
         if (vmfs_file_pread(b->f,buf,sizeof(buf),pos) != sizeof(buf))
            break;

         vmfs_bme_read(&entry,buf,0);

         if (entry.mdh.magic == 0)
            goto done;

         /* check the entry ID */
         if (entry.id != entry_id) {
            printf("Entry 0x%x has incorrect ID 0x%x\n",entry_id,entry.id);
            errors++;
         }
         
         /* check the magic number */
         if (magic == 0) {
            magic = entry.mdh.magic;
         } else {
            if (entry.mdh.magic != magic) {
               printf("Entry 0x%x has an incorrect magic id (0x%x)\n",
                      entry_id,entry.mdh.magic);
               errors++;
            }
         }
         
         /* check the number of items */
         if (entry.total > b->bmh.items_per_bitmap_entry) {
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
         pos += sizeof(buf);
      }
   }

 done:
   if (total_items != b->bmh.total_items) {
      printf("Total number of items (0x%x) doesn't match header info (0x%x)\n",
             total_items,b->bmh.total_items);
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

   if (vmfs_file_pread(f,buf,buf_len,0) != buf_len) {
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

vmfs_bitmap_t *vmfs_bitmap_open_at(vmfs_dir_t *d,const char *name)
{
   return vmfs_bitmap_open_from_file(vmfs_file_open_at(d, name));
}

vmfs_bitmap_t *vmfs_bitmap_open_from_inode(const vmfs_inode_t *inode)
{
   return vmfs_bitmap_open_from_file(vmfs_file_open_from_inode(inode));
}


/* Close a bitmap file */
void vmfs_bitmap_close(vmfs_bitmap_t *b)
{
   if (b != NULL) {
      vmfs_file_close(b->f);
      free(b);
   }
}
