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
 * VMFS inodes.
 */

#include <string.h>
#include <sys/stat.h>
#include "vmfs.h"

/* Read an inode */
int vmfs_inode_read(vmfs_inode_t *inode,const u_char *buf)
{
   inode->magic    = read_le32(buf,VMFS_INODE_OFS_MAGIC);
   inode->position = read_le64(buf,VMFS_INODE_OFS_POS);
   inode->hb_pos   = read_le64(buf,VMFS_INODE_OFS_HB_POS);
   inode->hb_lock  = read_le32(buf,VMFS_INODE_OFS_HB_LOCK);
   inode->id       = read_le32(buf,VMFS_INODE_OFS_ID);
   inode->id2      = read_le32(buf,VMFS_INODE_OFS_ID2);
   inode->type     = read_le32(buf,VMFS_INODE_OFS_TYPE);
   inode->size     = read_le64(buf,VMFS_INODE_OFS_SIZE);
   inode->mtime    = read_le32(buf,VMFS_INODE_OFS_MTIME);
   inode->ctime    = read_le32(buf,VMFS_INODE_OFS_CTIME);
   inode->atime    = read_le32(buf,VMFS_INODE_OFS_ATIME);
   inode->uid      = read_le32(buf,VMFS_INODE_OFS_UID);
   inode->gid      = read_le32(buf,VMFS_INODE_OFS_GID);
   inode->mode     = read_le32(buf,VMFS_INODE_OFS_MODE);

   /* "corrected" mode */
   inode->cmode    = inode->mode | vmfs_file_type2mode(inode->type);

   read_uuid(buf,VMFS_INODE_OFS_HB_UUID,&inode->hb_uuid);
   return(0);
}

/* Write an inode */
int vmfs_inode_write(const vmfs_inode_t *inode,u_char *buf)
{
   write_le32(buf,VMFS_INODE_OFS_MAGIC,inode->magic);
   write_le64(buf,VMFS_INODE_OFS_POS,inode->position);
   write_le64(buf,VMFS_INODE_OFS_HB_POS,inode->hb_pos);
   write_le32(buf,VMFS_INODE_OFS_HB_LOCK,inode->hb_lock);
   write_le32(buf,VMFS_INODE_OFS_ID,inode->id);
   write_le32(buf,VMFS_INODE_OFS_ID2,inode->id2);
   write_le32(buf,VMFS_INODE_OFS_TYPE,inode->type);
   write_le64(buf,VMFS_INODE_OFS_SIZE,inode->size);
   write_le32(buf,VMFS_INODE_OFS_MTIME,inode->mtime);
   write_le32(buf,VMFS_INODE_OFS_CTIME,inode->ctime);
   write_le32(buf,VMFS_INODE_OFS_ATIME,inode->atime);
   write_le32(buf,VMFS_INODE_OFS_UID,inode->uid);
   write_le32(buf,VMFS_INODE_OFS_GID,inode->gid);
   write_le32(buf,VMFS_INODE_OFS_MODE,inode->mode);
   return(0);
}

/* Show an inode */
void vmfs_inode_show(const vmfs_inode_t *inode)
{
   char tbuf[64];

   printf("  - Magic       : 0x%8.8x\n",inode->magic);
   printf("  - Position    : 0x%"PRIx64"\n",inode->position);
   printf("  - HB Position : 0x%"PRIx64"\n",inode->hb_pos);
   printf("  - HB Lock     : %d (%s)\n",
          inode->hb_lock,(inode->hb_lock > 0) ? "LOCKED":"UNLOCKED");
   printf("  - ID          : 0x%8.8x\n",inode->id);
   printf("  - ID2         : 0x%8.8x\n",inode->id2);
   printf("  - Type        : 0x%8.8x\n",inode->type);
   printf("  - Size        : 0x%8.8"PRIx64"\n",inode->size);
   printf("  - UID/GID     : %d/%d\n",inode->uid,inode->gid);
   printf("  - Mode        : 0%o (%s)\n",
          inode->mode,m_fmode_to_str(inode->mode,tbuf));
   printf("  - CMode       : 0%o (%s)\n",
          inode->cmode,m_fmode_to_str(inode->cmode,tbuf));

   printf("  - Access Time : %s\n",m_ctime(&inode->atime,tbuf,sizeof(tbuf)));
   printf("  - Modify Time : %s\n",m_ctime(&inode->mtime,tbuf,sizeof(tbuf)));
   printf("  - Change Time : %s\n",m_ctime(&inode->ctime,tbuf,sizeof(tbuf)));
}

/* Get the offset corresponding to an inode in the FDC file */
off_t vmfs_inode_get_offset(const vmfs_fs_t *fs,uint32_t blk_id)
{
   uint32_t subgroup,number;
   off_t inode_addr;
   uint32_t fdc_blk;

   subgroup = VMFS_BLK_FD_SUBGROUP(blk_id);
   number   = VMFS_BLK_FD_NUMBER(blk_id);

   /* Compute the address of the file meta-info in the FDC file */
   fdc_blk = subgroup * fs->fdc_bmh.items_per_bitmap_entry;
   inode_addr  = vmfs_bitmap_get_block_addr(&fs->fdc_bmh,fdc_blk);
   inode_addr += number * fs->fdc_bmh.data_size;

   return(inode_addr);
}

/* Get inode associated to a directory entry */
int vmfs_inode_get(const vmfs_fs_t *fs,const vmfs_dirent_t *rec,u_char *buf)
{
   uint32_t blk_id = rec->block_id;
   off_t inode_addr;
   ssize_t len;

   if (VMFS_BLK_TYPE(blk_id) != VMFS_BLK_TYPE_FD)
      return(-1);

   inode_addr = vmfs_inode_get_offset(fs,blk_id);

   if (vmfs_file_seek(fs->fdc,inode_addr,SEEK_SET) == -1)
      return(-1);
   
   len = vmfs_file_read(fs->fdc,buf,fs->fdc_bmh.data_size);
   return((len == fs->fdc_bmh.data_size) ? 0 : -1);
}

/* Resolve pointer blocks */
static int vmfs_inode_resolve_pb(vmfs_file_t *f,u_int base_pos,uint32_t blk_id)
{
   DECL_ALIGNED_BUFFER(buf,4096);
   const vmfs_bitmap_header_t *pbc_bmh;
   vmfs_file_t *pbc;
   uint32_t pbc_blk,dblk;
   uint32_t subgroup,number;
   size_t len;
   ssize_t res;
   off_t addr;
   int i;

   pbc = f->fs->pbc;
   pbc_bmh = &f->fs->pbc_bmh;

   subgroup = VMFS_BLK_PB_SUBGROUP(blk_id);
   number   = VMFS_BLK_PB_NUMBER(blk_id);

   /* Compute the address of the indirect pointers block in the PBC file */
   pbc_blk = (number * pbc_bmh->items_per_bitmap_entry) + subgroup;
   addr = vmfs_bitmap_get_block_addr(pbc_bmh,pbc_blk);
   len  = pbc_bmh->data_size;

   vmfs_file_seek(pbc,addr,SEEK_SET);

   while(len > 0) {
      res = vmfs_file_read(pbc,buf,buf_len);

      if (res != buf_len)
         return(-1);

      for(i=0;i<res/4;i++) {
         dblk = read_le32(buf,i*4);
         vmfs_blk_list_add_block(&f->blk_list,base_pos++,dblk);
      }

      len -= res;
   }

   return(0);
}

/* Bind inode info to a file */
int vmfs_inode_bind(vmfs_file_t *f,const u_char *inode_buf)
{
   uint32_t exp_blks,cur_pos = 0;
   uint32_t blk_id,blk_type;
   uint64_t blk_size;
   u_int icount;
   u_int factor;
   int i;

   vmfs_inode_read(&f->inode,inode_buf);

   blk_size = vmfs_fs_get_blocksize(f->fs);
   exp_blks = (f->inode.size + blk_size - 1) / blk_size;
   vmfs_blk_list_init(&f->blk_list,exp_blks);

   /* Indirect block count (in pointer blocks) */
   icount = f->fs->pbc_bmh.data_size / sizeof(uint32_t);
   factor = 1;

   for(i=0;i<VMFS_INODE_BLK_COUNT;i++) {
      blk_id   = read_le32(inode_buf,VMFS_INODE_OFS_BLK_ARRAY+(i*4));
      blk_type = VMFS_BLK_TYPE(blk_id);

      switch(blk_type) {
         /* COW block */
         case VMFS_BLK_TYPE_COW:
            cur_pos = (i + 1) * factor;
            break;
            
         /* Full-Block/Sub-Block: simply add it to the list */
         case VMFS_BLK_TYPE_FB:
         case VMFS_BLK_TYPE_SB:
            vmfs_blk_list_add_block(&f->blk_list,cur_pos++,blk_id);
            factor = 1;
            break;

         /* Pointer-block: resolve links */
         case VMFS_BLK_TYPE_PB:
            if (vmfs_inode_resolve_pb(f,i*icount,blk_id) == -1) {
               fprintf(stderr,"VMFS: unable to resolve blocks\n");
               return(-1);
            }

            cur_pos = (i + 1) * icount;
            break;

         default:
            fprintf(stderr,
                    "vmfs_file_bind_meta_info: "
                    "unexpected block type 0x%2.2x!\n",
                    blk_type);
            return(-1);
      }

      if (cur_pos >= exp_blks)
         break;
   }

   return(0);
}

/* Update inode info stored in memory */
int vmfs_inode_update_mem(vmfs_file_t *f)
{
   DECL_ALIGNED_BUFFER_WOL(inode_buf,VMFS_INODE_SIZE);

   vmfs_lvm_read(f->fs->lvm,f->inode.position,inode_buf,VMFS_INODE_SIZE);
   vmfs_inode_bind(f,inode_buf);
   return(0);
}

/* Get inode status */
int vmfs_inode_stat(const vmfs_inode_t *inode,struct stat *buf)
{
   memset(buf,0,sizeof(*buf));
   buf->st_mode  = inode->cmode;
   buf->st_nlink = 1;
   buf->st_uid   = inode->uid;
   buf->st_gid   = inode->gid;
   buf->st_size  = inode->size;
   buf->st_atime = inode->atime;
   buf->st_mtime = inode->mtime;
   buf->st_ctime = inode->ctime;
   return(0);
}
