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
int vmfs_bmh_read(vmfs_bitmap_header_t *bmh,u_char *buf)
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
void vmfs_bmh_show(vmfs_bitmap_header_t *bmh)
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
int vmfs_bme_read(vmfs_bitmap_entry_t *bme,u_char *buf,int copy_bitmap)
{
   bme->magic    = read_le32(buf,0x0);
   bme->position = read_le64(buf,0x4);
   bme->id       = read_le32(buf,0x200);
   bme->total    = read_le32(buf,0x204);
   bme->free     = read_le32(buf,0x208);
   bme->alloc    = read_le32(buf,0x20c);

   if (copy_bitmap) {
      memcpy(bme->bitmap,&buf[VMFS_BME_OFS_BITMAP ],(bme->total+7)/8);
   }

   return(0);
}

/* Get address of a block */
off_t vmfs_bitmap_get_block_addr(vmfs_bitmap_header_t *bmh,m_u32_t blk)
{
   m_u32_t items_per_area;
   off_t addr;
   u_int area;

   items_per_area = bmh->bmp_entries_per_area * bmh->items_per_bitmap_entry;
   area = blk / items_per_area;

   addr  = vmfs_bitmap_get_area_data_addr(bmh,area);
   addr += (blk % items_per_area) * bmh->data_size;

   return(addr);
}

/* Count the total number of allocated items in a bitmap area */
m_u32_t vmfs_bitmap_area_allocated_items(vmfs_file_t *f,
                                         vmfs_bitmap_header_t *bmh,
                                         u_int area)

{
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   m_u32_t count;
   int i;

   vmfs_file_seek(f,vmfs_bitmap_get_area_addr(bmh,area),SEEK_SET);

   for(i=0,count=0;i<bmh->bmp_entries_per_area;i++) {
      if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
         break;

      vmfs_bme_read(&entry,buf,0);
      count += entry.alloc;
   }

   return count;
}

/* Count the total number of allocated items in a bitmap */
m_u32_t vmfs_bitmap_allocated_items(vmfs_file_t *f,vmfs_bitmap_header_t *bmh)
{
   m_u32_t count;
   u_int i;
   
   for(i=0,count=0;i<bmh->area_count;i++)
      count += vmfs_bitmap_area_allocated_items(f,bmh,i);

   return(count);
}

/* Check coherency of a bitmap file */
int vmfs_bitmap_check(vmfs_file_t *f,vmfs_bitmap_header_t *bmh)
{  
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   m_u32_t total_items;
   m_u32_t magic;
   m_u32_t entry_id;
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
                   entry_id,bmap_count,entry.alloc);
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
