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

   fs->fs_info.block_size  = read_le32(buf,VMFS_FSINFO_OFS_BLKSIZE);

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

/* Read the root directory given its inode */
static int vmfs_read_rootdir(vmfs_fs_t *fs,u_char *inode_buf)
{
   if (!(fs->root_dir = vmfs_file_create_struct(fs)))
      return(-1);

   if (vmfs_inode_bind(fs->root_dir,inode_buf) == -1) {
      fprintf(stderr,"VMFS: unable to bind inode to root directory\n");
      return(-1);
   }
   
   return(0);
}

/* Read bitmap header */
static int vmfs_read_bitmap_header(vmfs_file_t *f,vmfs_bitmap_header_t *bmh)
{
   DECL_ALIGNED_BUFFER(buf,512);

   vmfs_file_seek(f,0,SEEK_SET);

   if (vmfs_file_read(f,buf,buf_len) != buf_len)
      return(-1);

   return(vmfs_bmh_read(bmh,buf));
}

/* Open a meta-file */
static vmfs_file_t *vmfs_open_meta_file(vmfs_fs_t *fs,char *name,
                                        vmfs_bitmap_header_t *bmh)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_INODE_SIZE);
   vmfs_dirent_t rec;
   vmfs_file_t *f;
   off_t inode_addr;

   if (!(f = vmfs_file_create_struct(fs)))
      return NULL;

   /* Search the file name in root directory */
   if (vmfs_dirent_search(fs->root_dir,name,&rec) != 1)
      return NULL;
   
   /* Read the inode info */
   inode_addr = vmfs_inode_get_offset(fs,rec.block_id);
   inode_addr += fs->fdc_base;

   if (vmfs_lvm_read(fs->lvm,inode_addr,buf,buf_len) != buf_len)
      return NULL;

   /* Bind the associated inode */
   if (vmfs_inode_bind(f,buf) == -1)
      return NULL;

   /* Read the bitmap header */
   if ((bmh != NULL) && (vmfs_read_bitmap_header(f,bmh) == -1))
      return NULL;
   
   return f;
}

/* Open all the VMFS meta files */
static int vmfs_open_all_meta_files(vmfs_fs_t *fs)
{
   fs->fbb = vmfs_open_meta_file(fs,VMFS_FBB_FILENAME,&fs->fbb_bmh);
   fs->fdc = vmfs_open_meta_file(fs,VMFS_FDC_FILENAME,&fs->fdc_bmh);
   fs->pbc = vmfs_open_meta_file(fs,VMFS_PBC_FILENAME,&fs->pbc_bmh);
   fs->sbc = vmfs_open_meta_file(fs,VMFS_SBC_FILENAME,&fs->sbc_bmh);

   return(0);
}

/* Dump volume bitmaps */
int vmfs_fs_dump_bitmaps(const vmfs_fs_t *fs)
{
   printf("FBB bitmap:\n");
   vmfs_bmh_show(&fs->fbb_bmh);

   printf("\nFDC bitmap:\n");
   vmfs_bmh_show(&fs->fdc_bmh);

   printf("\nPBC bitmap:\n");
   vmfs_bmh_show(&fs->pbc_bmh);

   printf("\nSBC bitmap:\n");
   vmfs_bmh_show(&fs->sbc_bmh);

   return(0);
}

/* Read FDC base information */
static int vmfs_read_fdc_base(vmfs_fs_t *fs)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_INODE_SIZE);
   off_t inode_pos;
   uint64_t len;

   /* Read the header */
   if (vmfs_lvm_read(fs->lvm,fs->fdc_base,buf,buf_len) != buf_len)
      return(-1);

   vmfs_bmh_read(&fs->fdc_bmh,buf);

   if (fs->debug_level > 0) {
      printf("FDC bitmap:\n");
      vmfs_bmh_show(&fs->fdc_bmh);
   }

   /* Read the first inode part */
   inode_pos = fs->fdc_base + vmfs_bitmap_get_area_data_addr(&fs->fdc_bmh,0);

   len = fs->fs_info.block_size - (inode_pos - fs->fdc_base);

   if (fs->debug_level > 0) {
      printf("Inodes at @0x%"PRIx64"\n",(uint64_t)inode_pos);
      printf("Length: 0x%8.8"PRIx64"\n",len);
   }

   /* Read the root directory */
   if (vmfs_lvm_read(fs->lvm,inode_pos,buf,fs->fdc_bmh.data_size)
       != fs->fdc_bmh.data_size)
      return(-1);

   vmfs_read_rootdir(fs,buf);

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

   /* Compute position of FDC base: it is located at the first
      block start after heartbeat information */
   fs->fdc_base = m_max(VMFS_HB_BASE + VMFS_HB_NUM * VMFS_HB_SIZE,
                        vmfs_fs_get_blocksize(fs));

   if (fs->debug_level > 0)
      printf("FDC base = @0x%"PRIx64"\n",(uint64_t)fs->fdc_base);

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
   vmfs_file_close(fs->fbb);
   vmfs_file_close(fs->fdc);
   vmfs_file_close(fs->pbc);
   vmfs_file_close(fs->sbc);
   vmfs_file_close(fs->root_dir);
   vmfs_lvm_close(fs->lvm);
   free(fs->fs_info.label);
   free(fs);
}
