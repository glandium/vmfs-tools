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

   return(vmfs_lvm_read(fs->lvm,pos,buf,len));
}

/* Write a block to the filesystem */
ssize_t vmfs_fs_write(const vmfs_fs_t *fs,uint32_t blk,off_t offset,
                      const u_char *buf,size_t len)
{
   off_t pos;

   pos  = (uint64_t)blk * vmfs_fs_get_blocksize(fs);
   pos += offset;

   return(vmfs_lvm_write(fs->lvm,pos,buf,len));
}

/* Read filesystem information */
static int vmfs_fsinfo_read(vmfs_fs_t *fs)
{
   DECL_ALIGNED_BUFFER(buf,512);

   if (vmfs_lvm_read(fs->lvm,VMFS_FSINFO_BASE,buf,buf_len) != buf_len)
      return(-1);

   fs->fs_info.magic = read_le32(buf,VMFS_FSINFO_OFS_MAGIC);

   if (fs->fs_info.magic != VMFS_FSINFO_MAGIC) {
      fprintf(stderr,"VMFS FSInfo: invalid magic number 0x%8.8x\n",
              fs->fs_info.magic);
      return(-1);
   }

   fs->fs_info.vol_version = read_le32(buf,VMFS_FSINFO_OFS_VOLVER);
   fs->fs_info.version     = buf[VMFS_FSINFO_OFS_VER];

   fs->fs_info.block_size  = read_le64(buf,VMFS_FSINFO_OFS_BLKSIZE);

   read_uuid(buf,VMFS_FSINFO_OFS_UUID,&fs->fs_info.uuid);
   fs->fs_info.label = strndup((char *)buf+VMFS_FSINFO_OFS_LABEL,
                               VMFS_FSINFO_OFS_LABEL_SIZE);
   read_uuid(buf,VMFS_FSINFO_OFS_LVM_UUID,&fs->fs_info.lvm_uuid);

   return(0);
}

/* Show FS information */
void vmfs_fs_show(const vmfs_fs_t *fs)
{  
   char uuid_str[M_UUID_BUFLEN];

   printf("VMFS FS Information:\n");

   printf("  - Vol. Version : %d\n",fs->fs_info.vol_version);
   printf("  - Version      : %d\n",fs->fs_info.version);
   printf("  - Label        : %s\n",fs->fs_info.label);
   printf("  - UUID         : %s\n",m_uuid_to_str(fs->fs_info.uuid,uuid_str));
   printf("  - Block size   : %"PRIu64" (0x%"PRIx64")\n",
          fs->fs_info.block_size,fs->fs_info.block_size);

   printf("\n");
}

/* Open all the VMFS meta files */
static int vmfs_open_all_meta_files(vmfs_fs_t *fs)
{
   vmfs_bitmap_t *fdc = fs->fdc;
   fs->fbb = vmfs_bitmap_open_from_path(fs,VMFS_FBB_FILENAME);
   fs->fdc = vmfs_bitmap_open_from_path(fs,VMFS_FDC_FILENAME);
   fs->pbc = vmfs_bitmap_open_from_path(fs,VMFS_PBC_FILENAME);
   fs->sbc = vmfs_bitmap_open_from_path(fs,VMFS_SBC_FILENAME);
   vmfs_bitmap_close(fdc);

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
   DECL_ALIGNED_BUFFER_WOL(buf,VMFS_INODE_SIZE);
   struct vmfs_inode_raw inode = { 0, };
   off_t fdc_base;
   uint32_t tmp;

   /* Compute position of FDC base: it is located at the first
      block start after heartbeat information */
   fdc_base = m_max(VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE,
                    vmfs_fs_get_blocksize(fs));

   if (fs->debug_level > 0)
      printf("FDC base = @0x%"PRIx64"\n",(uint64_t)fdc_base);

   /* read_le{32|64} is used as a mean to get little endian raw inode
    * data even on big endian platforms */
   inode.size = read_le64((u_char *)&fs->fs_info.block_size,0);
   tmp = VMFS_FILE_TYPE_META;
   inode.type = read_le32((u_char *)&tmp,0);
   tmp = VMFS_BLK_TYPE_FB + ((fdc_base / fs->fs_info.block_size) << 6);
   inode.blocks[0] = read_le32((u_char *)&tmp,0);

   fs->fdc = vmfs_bitmap_open_from_inode(fs,(u_char *)&inode);

   if (fs->debug_level > 0) {
      printf("FDC bitmap:\n");
      vmfs_bmh_show(&fs->fdc->bmh);
   }

   /* Read the first inode */
   if (!vmfs_bitmap_get_item(fs->fdc,0,0,buf)) {
      fprintf(stderr,"VMFS: couldn't read root directory inode\n");
      return(-1);
   }

   if (!(fs->root_dir = vmfs_file_open_from_inode(fs,buf))) {
      fprintf(stderr,"VMFS: unable to bind inode to root directory\n");
      return(-1);
   }

   /* Read the meta files */
   vmfs_open_all_meta_files(fs);

   /* Dump bitmap info */
   if (fs->debug_level > 0)
      vmfs_fs_dump_bitmaps(fs);

   return(0);
}

/* Create a FS structure */
vmfs_fs_t *vmfs_fs_create(vmfs_lvm_t *lvm)
{
   vmfs_fs_t *fs;

   if (!(fs = calloc(1,sizeof(*fs))))
      return NULL;

   fs->lvm = lvm;
   fs->debug_level = lvm->flags.debug_level;
   return fs;
}

/* Open a filesystem */
int vmfs_fs_open(vmfs_fs_t *fs)
{
   if (vmfs_lvm_open(fs->lvm))
      return(-1);

   /* Read FS info */
   if (vmfs_fsinfo_read(fs) == -1) {
      fprintf(stderr,"VMFS: Unable to read FS information\n");
      return(-1);
   }

   if (uuid_compare(fs->fs_info.lvm_uuid, fs->lvm->lvm_info.uuid)) {
      fprintf(stderr,"VMFS: FS doesn't belong to the underlying LVM\n");
      return(-1);
   }

   if (fs->debug_level > 0)
      vmfs_fs_show(fs);

   /* Read FDC base information */
   if (vmfs_read_fdc_base(fs) == -1) {
      fprintf(stderr,"VMFS: Unable to read FDC information\n");
      return(-1);
   }

   if (fs->debug_level > 0)
      printf("VMFS: filesystem opened successfully\n");
   return(0);
}

/* Close a FS */
void vmfs_fs_close(vmfs_fs_t *fs)
{
   if (!fs)
      return;
   vmfs_bitmap_close(fs->fbb);
   vmfs_bitmap_close(fs->fdc);
   vmfs_bitmap_close(fs->pbc);
   vmfs_bitmap_close(fs->sbc);
   vmfs_file_close(fs->root_dir);
   vmfs_lvm_close(fs->lvm);
   free(fs->fs_info.label);
   free(fs);
}
