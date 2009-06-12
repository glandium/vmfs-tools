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
 * VMFS blocks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include "utils.h"
#include "vmfs.h"

/* Get bitmap info (bitmap pointer,entry and item) from a block ID */
int vmfs_block_get_bitmap_info(const vmfs_fs_t *fs,uint32_t blk_id,
                               vmfs_bitmap_t **bmp,
                               uint32_t *entry,uint32_t *item)
{
   uint32_t blk_type;

   blk_type = VMFS_BLK_TYPE(blk_id);

   switch(blk_type) {
      /* File Block */
      case VMFS_BLK_TYPE_FB:
         *bmp   = fs->fbb;
         *entry = 0;
         *item  = VMFS_BLK_FB_ITEM(blk_id);
         break;

      /* Sub-Block */
      case VMFS_BLK_TYPE_SB:
         *bmp   = fs->sbc;
         *entry = VMFS_BLK_SB_ENTRY(blk_id);
         *item  = VMFS_BLK_SB_ITEM(blk_id);
         break;

      /* Pointer Block */
      case VMFS_BLK_TYPE_PB:
         *bmp   = fs->pbc;
         *entry = VMFS_BLK_PB_ENTRY(blk_id);
         *item  = VMFS_BLK_PB_ITEM(blk_id);
         break;

      /* Inode */
      case VMFS_BLK_TYPE_FD:
         *bmp   = fs->fdc;
         *entry = VMFS_BLK_FD_ENTRY(blk_id);
         *item  = VMFS_BLK_FD_ITEM(blk_id);
         break;

      default:
         return(-1);
   }

   return(0);
}

/* Get block status (allocated or free) */
int vmfs_block_get_status(const vmfs_fs_t *fs,uint32_t blk_id)
{
   vmfs_bitmap_entry_t entry;
   vmfs_bitmap_t *bmp;
   uint32_t blk_entry,blk_item;

   if (vmfs_block_get_bitmap_info(fs,blk_id,&bmp,&blk_entry,&blk_item) == -1)
      return(-1);

   if (vmfs_bitmap_get_entry(bmp,blk_entry,blk_item,&entry) == -1)
      return(-1);

   return(vmfs_bitmap_get_item_status(&bmp->bmh,&entry,blk_entry,blk_item));
}

/* Allocate or free the specified block */
static int vmfs_block_set_status(const vmfs_fs_t *fs,uint32_t blk_id,
                                 int status)
{
   vmfs_bitmap_entry_t entry;
   vmfs_bitmap_t *bmp;
   uint32_t blk_entry,blk_item;

   if (vmfs_block_get_bitmap_info(fs,blk_id,&bmp,&blk_entry,&blk_item) == -1)
      return(-1);

   if (vmfs_bitmap_get_entry(bmp,blk_entry,blk_item,&entry) == -1)
      return(-1);

   if (vmfs_bitmap_set_item_status(&bmp->bmh,&entry,
                                   blk_entry,blk_item,status) == -1)
      return(-1);

   return(vmfs_bme_update(fs,&entry));
}

/* Allocate the specified block */
int vmfs_block_alloc_specified(const vmfs_fs_t *fs,uint32_t blk_id)
{ 
   return(vmfs_block_set_status(fs,blk_id,1));
}

/* Free the specified block */
int vmfs_block_free(const vmfs_fs_t *fs,uint32_t blk_id)
{
   return(vmfs_block_set_status(fs,blk_id,0));
}
