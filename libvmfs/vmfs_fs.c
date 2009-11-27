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
 * VMFS filesystem..
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "vmfs.h"

/* VMFS meta-files */
#define VMFS_FBB_FILENAME  ".fbb.sf"
#define VMFS_FDC_FILENAME  ".fdc.sf"
#define VMFS_PBC_FILENAME  ".pbc.sf"
#define VMFS_SBC_FILENAME  ".sbc.sf"

/* Read a block from the filesystem */
ssize_t vmfs_fs_read(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                     u_char *buf,size_t len)
{
   off_t pos;

   pos  = (uint64_t)blk * vmfs_fs_get_blocksize(fs);
   pos += offset;

   return(vmfs_device_read(fs->dev,pos,buf,len));
}

/* Write a block to the filesystem */
ssize_t vmfs_fs_write(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                      const u_char *buf,size_t len)
{
   off_t pos;

   pos  = (uint64_t)blk * vmfs_fs_get_blocksize(fs);
   pos += offset;

   return(vmfs_device_write(fs->dev,pos,buf,len));
}

/* Read filesystem information */
static int vmfs_fsinfo_read(vmfs_fs_t *fs)
{
   DECL_ALIGNED_BUFFER(buf,512);
   vmfs_fsinfo_t *fsi = &fs->fs_info;

   if (vmfs_device_read(fs->dev,VMFS_FSINFO_BASE,buf,buf_len) != buf_len)
      return(-1);

   fsi->magic = read_le32(buf,VMFS_FSINFO_OFS_MAGIC);

   if (fsi->magic != VMFS_FSINFO_MAGIC) {
      fprintf(stderr,"VMFS FSInfo: invalid magic number 0x%8.8x\n",fsi->magic);
      return(-1);
   }

   fsi->vol_version      = read_le32(buf,VMFS_FSINFO_OFS_VOLVER);
   fsi->version          = buf[VMFS_FSINFO_OFS_VER];
   fsi->mode             = read_le32(buf,VMFS_FSINFO_OFS_MODE);
   fsi->block_size       = read_le64(buf,VMFS_FSINFO_OFS_BLKSIZE);
   fsi->subblock_size    = read_le32(buf,VMFS_FSINFO_OFS_SBSIZE);
   fsi->fdc_header_size  = read_le32(buf,VMFS_FSINFO_OFS_FDC_HEADER_SIZE);
   fsi->fdc_bitmap_count = read_le32(buf,VMFS_FSINFO_OFS_FDC_BITMAP_COUNT);
   fsi->ctime            = (time_t)read_le32(buf,VMFS_FSINFO_OFS_CTIME);

   read_uuid(buf,VMFS_FSINFO_OFS_UUID,&fsi->uuid);
   fsi->label = strndup((char *)buf+VMFS_FSINFO_OFS_LABEL,
                        VMFS_FSINFO_OFS_LABEL_SIZE);
   read_uuid(buf,VMFS_FSINFO_OFS_LVM_UUID,&fsi->lvm_uuid);

   return(0);
}

/* Get string corresponding to specified mode */
static char *vmfs_fs_mode_to_str(uint32_t mode)
{
   /* only two lower bits seem to be significant */
   switch(mode & 0x03) {
      case 0x00:
         return "private";
      case 0x01:
      case 0x03:
         return "shared";
      case 0x02:
         return "public";        
   }
   
   /* should not happen */
   return NULL;
}

/* Show FS information */
void vmfs_fs_show(const vmfs_fs_t *fs)
{  
   char uuid_str[M_UUID_BUFLEN];
   char tbuf[64];

   printf("VMFS FS Information:\n");

   printf("  - Volume Version   : %d\n",fs->fs_info.vol_version);
   printf("  - Version          : %d\n",fs->fs_info.version);
   printf("  - Label            : %s\n",fs->fs_info.label);
   printf("  - Mode             : %s\n",vmfs_fs_mode_to_str(fs->fs_info.mode));
   printf("  - UUID             : %s\n",
          m_uuid_to_str(fs->fs_info.uuid,uuid_str));
   printf("  - Creation time    : %s\n",
          m_ctime(&fs->fs_info.ctime,tbuf,sizeof(tbuf)));
   printf("  - Block size       : %"PRIu64" (0x%"PRIx64")\n",
          fs->fs_info.block_size,fs->fs_info.block_size);
   printf("  - Subblock size    : %u (0x%x)\n",
          fs->fs_info.subblock_size,fs->fs_info.subblock_size);
   printf("  - FDC Header size  : 0x%x\n", fs->fs_info.fdc_header_size);
   printf("  - FDC Bitmap count : 0x%x\n", fs->fs_info.fdc_bitmap_count);

   printf("\n");
}

/* Open all the VMFS meta files */
static int vmfs_open_all_meta_files(vmfs_fs_t *fs)
{
   vmfs_bitmap_t *fdc = fs->fdc;
   vmfs_dir_t *root_dir;

   /* Read the first inode */
   if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0)))) {
      fprintf(stderr,"VMFS: unable to open root directory\n");
      return(-1);
   }

   if (!(fs->fbb = vmfs_bitmap_open_at(root_dir,VMFS_FBB_FILENAME))) {
      fprintf(stderr,"Unable to open file-block bitmap (FBB).\n");
      return(-1);
   }

   if (!(fs->fdc = vmfs_bitmap_open_at(root_dir,VMFS_FDC_FILENAME))) {
      fprintf(stderr,"Unable to open file descriptor bitmap (FDC).\n");
      return(-1);
   }

   if (!(fs->pbc = vmfs_bitmap_open_at(root_dir,VMFS_PBC_FILENAME))) {
      fprintf(stderr,"Unable to open pointer block bitmap (PBC).\n");
      return(-1);
   }

   if (!(fs->sbc = vmfs_bitmap_open_at(root_dir,VMFS_SBC_FILENAME))) {
      fprintf(stderr,"Unable to open sub-block bitmap (SBC).\n");
      return(-1);
   }

   vmfs_bitmap_close(fdc);
   vmfs_dir_close(root_dir);
   return(0);
}

/* Dump volume bitmaps */
int vmfs_fs_dump_bitmaps(const vmfs_fs_t *fs)
{
   printf("FBB bitmap:\n");
   vmfs_bmh_show(&fs->fbb->bmh);

   printf("\nFDC bitmap:\n");
   vmfs_bmh_show(&fs->fdc->bmh);

   printf("\nPBC bitmap:\n");
   vmfs_bmh_show(&fs->pbc->bmh);

   printf("\nSBC bitmap:\n");
   vmfs_bmh_show(&fs->sbc->bmh);

   return(0);
}

/* Read FDC base information */
static int vmfs_read_fdc_base(vmfs_fs_t *fs)
{
   vmfs_inode_t inode = { { 0, }, };
   uint32_t fdc_base;

   /* 
    * Compute position of FDC base: it is located at the first
    * block after heartbeat information.
    * When blocksize = 8 Mb, there is free space between heartbeats
    * and FDC.
    */
   fdc_base = m_max(1, (VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE) /
                    vmfs_fs_get_blocksize(fs));

   if (fs->debug_level > 0)
      printf("FDC base = block #%u\n", fdc_base);

   inode.fs = fs;
   inode.mdh.magic = VMFS_INODE_MAGIC;
   inode.size = fs->fs_info.block_size;
   inode.type = VMFS_FILE_TYPE_META;
   inode.blk_size = fs->fs_info.block_size;
   inode.blk_count = 1;
   inode.zla = VMFS_BLK_TYPE_FB;
   inode.blocks[0] = VMFS_BLK_FB_BUILD(fdc_base);
   inode.ref_count = 1;

   fs->fdc = vmfs_bitmap_open_from_inode(&inode);

   if (fs->debug_level > 0) {
      printf("FDC bitmap:\n");
      vmfs_bmh_show(&fs->fdc->bmh);
   }

   /* Read the meta files */
   if (vmfs_open_all_meta_files(fs) == -1)
      return(-1);

   /* Dump bitmap info */
   if (fs->debug_level > 0)
      vmfs_fs_dump_bitmaps(fs);

   return(0);
}

static vmfs_device_t *vmfs_device_open(char **paths, vmfs_flags_t flags)
{
   vmfs_lvm_t *lvm;

   if (!(lvm = vmfs_lvm_create(flags))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      return NULL;
   }

   for (; *paths; paths++) {
      if (vmfs_lvm_add_extent(lvm, vmfs_vol_open(*paths, flags)) == -1) {
         fprintf(stderr,"Unable to open device/file \"%s\".\n",*paths);
         return NULL;
      }
   }

   if (vmfs_lvm_open(lvm)) {
      vmfs_device_close(&lvm->dev);
      return NULL;
   }

   return &lvm->dev;
}

/* Open a filesystem */
vmfs_fs_t *vmfs_fs_open(char **paths, vmfs_flags_t flags)
{
   vmfs_device_t *dev;
   vmfs_fs_t *fs;

   vmfs_host_init();

   dev = vmfs_device_open(paths, flags);

   if (!dev || !(fs = calloc(1,sizeof(*fs))))
      return NULL;

   fs->inode_hash_buckets = VMFS_INODE_HASH_BUCKETS;
   fs->inodes = calloc(fs->inode_hash_buckets,sizeof(vmfs_inode_t *));

   if (!fs->inodes) {
      free(fs);
      return NULL;
   }

   fs->dev = dev;
   fs->debug_level = flags.debug_level;

   /* Read FS info */
   if (vmfs_fsinfo_read(fs) == -1) {
      fprintf(stderr,"VMFS: Unable to read FS information\n");
      vmfs_fs_close(fs);
      return NULL;
   }

   if (uuid_compare(fs->fs_info.lvm_uuid, *fs->dev->uuid)) {
      fprintf(stderr,"VMFS: FS doesn't belong to the underlying LVM\n");
      vmfs_fs_close(fs);
      return NULL;
   }

   if (fs->debug_level > 0)
      vmfs_fs_show(fs);

   /* Read FDC base information */
   if (vmfs_read_fdc_base(fs) == -1) {
      fprintf(stderr,"VMFS: Unable to read FDC information\n");
      vmfs_fs_close(fs);
      return NULL;
   }

   if (fs->debug_level > 0)
      printf("VMFS: filesystem opened successfully\n");
   return fs;
}

/* 
 * Check that all inodes have been released, and synchronize them if this 
 * is not the case. 
 */
static void vmfs_fs_sync_inodes(vmfs_fs_t *fs)
{
   vmfs_inode_t *inode;
   int i;

   for(i=0;i<VMFS_INODE_HASH_BUCKETS;i++) {
      for(inode=fs->inodes[i];inode;inode=inode->next) {
#if 0
         printf("Inode 0x%8.8x: ref_count=%u, update_flags=0x%x\n",
                inode->id,inode->ref_count,inode->update_flags);
#endif
         if (inode->update_flags)
            vmfs_inode_update(inode,inode->update_flags & VMFS_INODE_SYNC_BLK);
      }
   }
}

/* Close a FS */
void vmfs_fs_close(vmfs_fs_t *fs)
{
   if (!fs)
      return;

   if (fs->hb_refcount > 0) {
      fprintf(stderr,
              "Warning: heartbeat still active in metadata (ref_count=%u)\n",
              fs->hb_refcount);
   }

   vmfs_heartbeat_unlock(fs,&fs->hb);

   vmfs_bitmap_close(fs->fbb);
   vmfs_bitmap_close(fs->fdc);
   vmfs_bitmap_close(fs->pbc);
   vmfs_bitmap_close(fs->sbc);

   vmfs_fs_sync_inodes(fs);

   vmfs_device_close(fs->dev);
   free(fs->inodes);
   free(fs->fs_info.label);
   free(fs);
}
