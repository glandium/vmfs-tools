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

/* Initialize a block list */
int vmfs_blk_list_init(vmfs_blk_list_t *list,m_u32_t blk_count)
{
   if (!(list->blk_id = calloc(blk_count,sizeof(m_u32_t))))
      return(-1);

   list->total = list->last_pos = blk_count;   
   return(0);
}

/* Free a block list */
void vmfs_blk_list_free(vmfs_blk_list_t *list)
{
   free(list->blk_id);
   list->blk_id   = NULL;
   list->total    = 0;
   list->last_pos = 0;
}

/* Set a block at the specified position */
int vmfs_blk_list_add_block(vmfs_blk_list_t *list,u_int pos,m_u32_t blk_id)
{
   size_t new_size;
   m_u32_t new_total;
   void *ptr;
   
   if (pos < list->total) {
      list->blk_id[pos] = blk_id;
      return(0);
   }

   new_total = pos + 128;
   new_size  = new_total * sizeof(m_u32_t);

   if (!(ptr = realloc(list->blk_id,new_size)))
      return(-1);

   list->blk_id   = ptr;
   list->total    = new_total;
   list->last_pos = pos + 1;
   return(0);
}

/* Get a block ID from a block list, given its position */
int vmfs_blk_list_get_block(const vmfs_blk_list_t *list,u_int pos,m_u32_t *blk_id)
{
   if (pos > list->total)
      return(-1);

   *blk_id = list->blk_id[pos];
   return(0);
}

/* Show a block list */
void vmfs_blk_list_show(const vmfs_blk_list_t *list)
{
   int i;

   for(i=0;i<list->last_pos;i++) {
      printf("0x%8.8x ",list->blk_id[i]);
      if (((i+1) % 4) == 0) printf("\n");
   }

   printf("\n");
}
