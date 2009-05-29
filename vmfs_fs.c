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
   vmfs_fsinfo_t *fsi = &fs->fs_info;

   if (vmfs_lvm_read(fs->lvm,VMFS_FSINFO_BASE,buf,buf_len) != buf_len)
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

   if (!(fs->fbb = vmfs_bitmap_open_from_path(fs,VMFS_FBB_FILENAME))) {
      fprintf(stderr,"Unable to open file-block bitmap (FBB).\n");
      return(-1);
   }

   if (!(fs->fdc = vmfs_bitmap_open_from_path(fs,VMFS_FDC_FILENAME))) {
      fprintf(stderr,"Unable to open file descriptor bitmap (FDC).\n");
      return(-1);
   }

   if (!(fs->pbc = vmfs_bitmap_open_from_path(fs,VMFS_PBC_FILENAME))) {
      fprintf(stderr,"Unable to open pointer block bitmap (PBC).\n");
      return(-1);
   }

   if (!(fs->sbc = vmfs_bitmap_open_from_path(fs,VMFS_SBC_FILENAME))) {
      fprintf(stderr,"Unable to open sub-block bitmap (SBC).\n");
      return(-1);
   }

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
   struct vmfs_inode_raw inode = { { 0, }, };
   off_t fdc_base;
   uint32_t tmp;

   /* 
    * Compute position of FDC base: it is located at the first
    * block after heartbeat information.
    * When blocksize = 8 Mb, there is free space between heartbeats
    * and FDC.
    */
   fdc_base = m_max(VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE,
                    vmfs_fs_get_blocksize(fs));

   if (fs->debug_level > 0)
      printf("FDC base = @0x%"PRIx64"\n",(uint64_t)fdc_base);

   /* read_le{32|64} is used as a mean to get little endian raw inode
    * data even on big endian platforms */
   inode.size = read_le64((u_char *)&fs->fs_info.block_size,0);
   tmp = VMFS_FILE_TYPE_META;
   inode.type = read_le32((u_char *)&tmp,0);
   inode.blk_size = read_le64((u_char *)&fs->fs_info.block_size,0);
   tmp = 1;
   inode.blk_count = read_le32((u_char *)&tmp,0);
   tmp = VMFS_BLK_TYPE_FB;
   inode.zla = read_le32((u_char *)&tmp,0);
   tmp += (fdc_base / fs->fs_info.block_size) << 6;
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
   if (vmfs_open_all_meta_files(fs) == -1)
      return(-1);

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
