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

typedef struct vmfs_fsck_info vmfs_fsck_info_t;
struct vmfs_fsck_info {
   vmfs_blk_map_t *blk_map[VMFS_BLK_MAP_BUCKETS];
   u_int blk_count[VMFS_BLK_TYPE_MAX];

   /* Inodes referenced in directory structure but not in FDC */
   u_int undef_inodes;
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
int vmfs_fsck_get_all_block_mappings(const vmfs_fs_t *fs,
                                     vmfs_fsck_info_t *fi)
{
   DECL_ALIGNED_BUFFER_WOL(inode_buf,VMFS_INODE_SIZE);
   vmfs_inode_t inode;
   vmfs_bitmap_header_t *fdc_bmp;
   uint32_t entry,item;
   int i;

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

      vmfs_fsck_store_inode(fs,fi->blk_map,&inode);
      vmfs_inode_foreach_block(fs,&inode,vmfs_fsck_store_block,fi->blk_map);
   }

   return(0);
}

/* Display Inode IDs for blocks incorrectly shared by multiple inodes */
void vmfs_fsck_show_inode_id(vmfs_blk_map_t *map)
{
   u_int i,inode_count;

   inode_count = m_min(map->ref_count,VMFS_BLK_MAP_MAX_INODES);

   for(i=0;i<inode_count;i++)
      printf("0x%8.8x ",map->inode_id[i]);

   printf("\n");
}

/* Count block types */
void vmfs_fsck_count_blocks(vmfs_fsck_info_t *fi)
{
   vmfs_blk_map_t *map;
   u_int blk_type;
   int i;

   for(i=0;i<VMFS_BLK_MAP_BUCKETS;i++) {
      for(map=fi->blk_map[i];map;map=map->next) {
         blk_type = VMFS_BLK_TYPE(map->blk_id);

         if ((blk_type != VMFS_BLK_TYPE_FD) && (map->ref_count > 1)) {
            printf("Block 0x%8.8x is referenced by multiple inodes: \n",
                   map->blk_id);
            vmfs_fsck_show_inode_id(map);
         }

         if (blk_type < VMFS_BLK_TYPE_MAX)
            fi->blk_count[blk_type]++;
      }
   }
   
   printf("Data collected from inode entries:\n");   
   printf("  File Blocks    : %u\n",fi->blk_count[VMFS_BLK_TYPE_FB]);
   printf("  Sub-Blocks     : %u\n",fi->blk_count[VMFS_BLK_TYPE_SB]);
   printf("  Pointer Blocks : %u\n",fi->blk_count[VMFS_BLK_TYPE_PB]);
   printf("  Inodes         : %u\n\n",fi->blk_count[VMFS_BLK_TYPE_FD]);
}

/* 
 * Walk recursively through the directory structure and check inode 
 * allocation.
 */
int vmfs_fsck_walk_dir(const vmfs_fs_t *fs,
                       vmfs_fsck_info_t *fi,
                       vmfs_file_t *dir_entry)
{   
   u_char buf[VMFS_DIRENT_SIZE];
   vmfs_dirent_t rec;
   vmfs_file_t *sub_dir;
   vmfs_blk_map_t *map;
   int i,res,dir_count;
   ssize_t len;

   dir_count = vmfs_file_get_size(dir_entry) / VMFS_DIRENT_SIZE;
   vmfs_file_seek(dir_entry,0,SEEK_SET);

   for(i=0;i<dir_count;i++) {
      len = vmfs_file_read(dir_entry,buf,sizeof(buf));
      
      if (len != sizeof(buf))
         return(-1);

      vmfs_dirent_read(&rec,buf);

      if (rec.type == VMFS_FILE_TYPE_DIR) {
         if (strcmp(rec.name,".") && strcmp(rec.name,"..")) {
            if (!(sub_dir = vmfs_file_open_from_rec(fs,&rec)))
               return(-1);

            res = vmfs_fsck_walk_dir(fs,fi,sub_dir);
            vmfs_file_close(sub_dir);
         
            if (res == -1)
               return(-1);
         }
      }

      if (!(map = vmfs_block_map_find(fi->blk_map,rec.block_id))) {
         fi->undef_inodes++;
      }
   }

   return(0);
}

/* Initialize fsck structures */
static void vmfs_fsck_init(vmfs_fsck_info_t *fi)
{
   memset(fi,0,sizeof(*fi));
}

static void show_usage(char *prog_name) 
{
   char *name = basename(prog_name);

   fprintf(stderr,"%s " VERSION "\n",name);
   fprintf(stderr,"Syntax: %s <device_name...>\n\n",name);
}

int main(int argc,char *argv[])
{
   vmfs_fsck_info_t fsck_info;
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

   vmfs_fsck_init(&fsck_info);
   vmfs_fsck_get_all_block_mappings(fs,&fsck_info);
   vmfs_fsck_walk_dir(fs,&fsck_info,fs->root_dir);

   vmfs_fsck_count_blocks(&fsck_info);

   printf("Undefined inodes: %u\n",fsck_info.undef_inodes);

   vmfs_fs_close(fs);
   return(0);
}
