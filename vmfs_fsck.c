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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <libgen.h>
#include "vmfs.h"
#include "readcmd.h"

/* 
 * Block mapping, which allows to keep track of inodes given a block number.
 * Used for troubleshooting/debugging/future fsck.
 */
#define VMFS_BLK_MAP_BUCKETS     512
#define VMFS_BLK_MAP_MAX_INODES  32

typedef struct vmfs_blk_map vmfs_blk_map_t;
struct vmfs_blk_map {
   uint32_t blk_id;
   union {
      uint32_t inode_id[VMFS_BLK_MAP_MAX_INODES];
      vmfs_inode_t inode;
   };
   u_int ref_count;
   int status;
   vmfs_blk_map_t *prev,*next;
};

/* Hash function for a block ID */
static inline u_int vmfs_block_map_hash(uint32_t blk_id)
{
   return(blk_id ^ (blk_id >> 5));
}

/* Find a block mapping */
vmfs_blk_map_t *vmfs_block_map_find(vmfs_blk_map_t **ht,uint32_t blk_id)
{   
   vmfs_blk_map_t *map;
   u_int bucket;

   bucket = vmfs_block_map_hash(blk_id) % VMFS_BLK_MAP_BUCKETS;

   for(map=ht[bucket];map;map=map->next)
      if (map->blk_id == blk_id)
         return map;

   return NULL;
}

/* Get a block mapping */
vmfs_blk_map_t *vmfs_block_map_get(vmfs_blk_map_t **ht,uint32_t blk_id)
{
   vmfs_blk_map_t *map;
   u_int bucket;

   if ((map = vmfs_block_map_find(ht,blk_id)) != NULL)
      return map;

   if (!(map = calloc(1,sizeof(*map))))
      return NULL;

   bucket = vmfs_block_map_hash(blk_id) % VMFS_BLK_MAP_BUCKETS;

   map->blk_id = blk_id;

   map->prev = NULL;
   map->next = ht[bucket];
   ht[bucket] = map;

   return map;
}

/* Store block mapping of an inode */
static void vmfs_fsck_store_block(const vmfs_fs_t *fs,
                                  const vmfs_inode_t *inode,
                                  uint32_t pb_blk,
                                  uint32_t blk_id,
                                  void *opt_arg)
{
   vmfs_blk_map_t **ht = opt_arg;
   vmfs_blk_map_t *map;

   if (!(map = vmfs_block_map_get(ht,blk_id)))
      return;
   
   if (map->ref_count < VMFS_BLK_MAP_MAX_INODES)
      map->inode_id[map->ref_count] = inode->id;

   map->ref_count++;
   map->status = vmfs_block_get_status(fs,blk_id);
}

/* Store inode info */
static int vmfs_fsck_store_inode(const vmfs_fs_t *fs,vmfs_blk_map_t **ht,
                                 const vmfs_inode_t *inode)
{
   vmfs_blk_map_t *map;

   if (!(map = vmfs_block_map_get(ht,inode->id)))
      return(-1);
   
   memcpy(&map->inode,inode,sizeof(*inode));
   return(0);
}

/* Iterate over all inodes of the FS and get all block mappings  */
int vmfs_fsck_get_all_block_mappings(const vmfs_fs_t *fs)
{
   DECL_ALIGNED_BUFFER_WOL(inode_buf,VMFS_INODE_SIZE);
   vmfs_inode_t inode;
   vmfs_bitmap_header_t *fdc_bmp;
   vmfs_blk_map_t *ht[VMFS_BLK_MAP_BUCKETS];
   vmfs_blk_map_t *map;
   uint32_t entry,item;
   u_int blk_count[VMFS_BLK_TYPE_MAX] = { 0, };
   int i;

   memset(ht,0,sizeof(ht));
   fdc_bmp = &fs->fdc->bmh;

   printf("Scanning %u FDC entries...\n",fdc_bmp->total_items);

   for(i=0;i<fdc_bmp->total_items;i++) {
      entry = i / fdc_bmp->items_per_bitmap_entry;
      item  = i % fdc_bmp->items_per_bitmap_entry;

      if (!vmfs_bitmap_get_item(fs->fdc,entry,item,inode_buf)) {        
         fprintf(stderr,"Unable to read inode (%u,%u)\n",entry,item);
         return(-1);
      }

      /* Skip undefined/deleted inodes */
      if ((vmfs_inode_read(&inode,inode_buf) == -1) || !inode.nlink)
         continue;

      blk_count[VMFS_BLK_TYPE_FD]++;
      vmfs_fsck_store_inode(fs,ht,&inode);
      vmfs_inode_foreach_block(fs,&inode,vmfs_fsck_store_block,ht);
   }

   for(i=0;i<VMFS_BLK_MAP_BUCKETS;i++)
      for(map=ht[i];map;map=map->next) {
         u_int blk_type;

         if (map->ref_count > 1) {
            fprintf(stderr,"Block 0x%8.8x has multiple references\n",
                    map->blk_id);
         }

         blk_type = VMFS_BLK_TYPE(map->blk_id);
         if (blk_type < VMFS_BLK_TYPE_MAX)
            blk_count[blk_type]++;
      }

   printf("File Blocks    : %u\n",blk_count[VMFS_BLK_TYPE_FB]);
   printf("Sub-Blocks     : %u\n",blk_count[VMFS_BLK_TYPE_SB]);
   printf("Pointer Blocks : %u\n",blk_count[VMFS_BLK_TYPE_PB]);
   printf("Inodes         : %u\n",blk_count[VMFS_BLK_TYPE_FD]);

   return(0);
}

static void show_usage(char *prog_name) 
{
   char *name = basename(prog_name);

   fprintf(stderr,"%s " VERSION "\n",name);
   fprintf(stderr,"Syntax: %s <device_name...>\n\n",name);
}

int main(int argc,char *argv[])
{
   vmfs_lvm_t *lvm;
   vmfs_fs_t *fs;
   vmfs_flags_t flags;
   int i;

   if (argc < 2) {
      show_usage(argv[0]);
      return(0);
   }

   flags.packed = 0;

   if (!(lvm = vmfs_lvm_create(flags))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      exit(EXIT_FAILURE);
   }

   for(i=1;i<argc;i++) {
      if (vmfs_lvm_add_extent(lvm,argv[i]) == -1) {
         fprintf(stderr,"Unable to open device/file \"%s\".\n",argv[i]);
         exit(EXIT_FAILURE);
      }
   }

   if (!(fs = vmfs_fs_create(lvm))) {
      fprintf(stderr,"Unable to open filesystem\n");
      exit(EXIT_FAILURE);
   }
   
   if (vmfs_fs_open(fs) == -1) {
      fprintf(stderr,"Unable to open volume.\n");
      exit(EXIT_FAILURE);
   }

   vmfs_fsck_get_all_block_mappings(fs);

   vmfs_fs_close(fs);
   return(0);
}
