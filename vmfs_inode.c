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
   vmfs_metadata_hdr_read(&inode->mdh,buf);
   inode->id        = read_le32(buf,VMFS_INODE_OFS_ID);
   inode->id2       = read_le32(buf,VMFS_INODE_OFS_ID2);
   inode->nlink     = read_le32(buf,VMFS_INODE_OFS_NLINK);
   inode->type      = read_le32(buf,VMFS_INODE_OFS_TYPE);
   inode->flags     = read_le32(buf,VMFS_INODE_OFS_FLAGS);
   inode->size      = read_le64(buf,VMFS_INODE_OFS_SIZE);
   inode->blk_size  = read_le64(buf,VMFS_INODE_OFS_BLK_SIZE);
   inode->blk_count = read_le64(buf,VMFS_INODE_OFS_BLK_COUNT);
   inode->mtime     = read_le32(buf,VMFS_INODE_OFS_MTIME);
   inode->ctime     = read_le32(buf,VMFS_INODE_OFS_CTIME);
   inode->atime     = read_le32(buf,VMFS_INODE_OFS_ATIME);
   inode->uid       = read_le32(buf,VMFS_INODE_OFS_UID);
   inode->gid       = read_le32(buf,VMFS_INODE_OFS_GID);
   inode->mode      = read_le32(buf,VMFS_INODE_OFS_MODE);
   inode->zla       = read_le32(buf,VMFS_INODE_OFS_ZLA);
   inode->tbz       = read_le32(buf,VMFS_INODE_OFS_TBZ);
   inode->cow       = read_le32(buf,VMFS_INODE_OFS_COW);

   /* "corrected" mode */
   inode->cmode    = inode->mode | vmfs_file_type2mode(inode->type);

   if (inode->type == VMFS_FILE_TYPE_RDM) {
      inode->rdm_id = read_le32(buf,VMFS_INODE_OFS_RDM_ID);
   }

   return(0);
}

/* Write an inode */
int vmfs_inode_write(const vmfs_inode_t *inode,u_char *buf)
{
   vmfs_metadata_hdr_write(&inode->mdh,buf);
   write_le32(buf,VMFS_INODE_OFS_ID,inode->id);
   write_le32(buf,VMFS_INODE_OFS_ID2,inode->id2);
   write_le32(buf,VMFS_INODE_OFS_NLINK,inode->nlink);
   write_le32(buf,VMFS_INODE_OFS_TYPE,inode->type);
   write_le32(buf,VMFS_INODE_OFS_FLAGS,inode->flags);
   write_le64(buf,VMFS_INODE_OFS_SIZE,inode->size);
   write_le64(buf,VMFS_INODE_OFS_BLK_SIZE,inode->blk_size);
   write_le64(buf,VMFS_INODE_OFS_BLK_COUNT,inode->blk_count);
   write_le32(buf,VMFS_INODE_OFS_MTIME,inode->mtime);
   write_le32(buf,VMFS_INODE_OFS_CTIME,inode->ctime);
   write_le32(buf,VMFS_INODE_OFS_ATIME,inode->atime);
   write_le32(buf,VMFS_INODE_OFS_UID,inode->uid);
   write_le32(buf,VMFS_INODE_OFS_GID,inode->gid);
   write_le32(buf,VMFS_INODE_OFS_MODE,inode->mode);
   write_le32(buf,VMFS_INODE_OFS_ZLA,inode->zla);
   write_le32(buf,VMFS_INODE_OFS_TBZ,inode->tbz);
   write_le32(buf,VMFS_INODE_OFS_COW,inode->cow);
   return(0);
}

/* Show an inode */
void vmfs_inode_show(const vmfs_inode_t *inode)
{
   char tbuf[64];

   vmfs_metadata_hdr_show(&inode->mdh);

   printf("  - ID           : 0x%8.8x\n",inode->id);
   printf("  - ID2          : 0x%8.8x\n",inode->id2);
   printf("  - Links        : %u\n",inode->nlink);
   printf("  - Type         : 0x%8.8x\n",inode->type);
   printf("  - Flags        : 0x%8.8x\n",inode->flags);
   printf("  - Size         : 0x%8.8"PRIx64"\n",inode->size);
   printf("  - Block size   : 0x%"PRIx64"\n",inode->blk_size);
   printf("  - Block count  : 0x%"PRIx64"\n",inode->blk_count);
   printf("  - UID/GID      : %d/%d\n",inode->uid,inode->gid);
   printf("  - Mode         : 0%o (%s)\n",
          inode->mode,m_fmode_to_str(inode->mode,tbuf));
   printf("  - CMode        : 0%o (%s)\n",
          inode->cmode,m_fmode_to_str(inode->cmode,tbuf));
   printf("  - ZLA          : 0x%8.8x\n",inode->zla);
   printf("  - TBZ          : 0x%8.8x\n",inode->tbz);
   printf("  - COW          : 0x%8.8x\n",inode->cow);

   printf("  - Access Time  : %s\n",m_ctime(&inode->atime,tbuf,sizeof(tbuf)));
   printf("  - Modify Time  : %s\n",m_ctime(&inode->mtime,tbuf,sizeof(tbuf)));
   printf("  - Change Time  : %s\n",m_ctime(&inode->ctime,tbuf,sizeof(tbuf)));

   if (inode->type == VMFS_FILE_TYPE_RDM) {
      printf("  - RDM ID       : 0x%8.8x\n",inode->rdm_id);
   }
}

/* Get inode associated to a directory entry */
int vmfs_inode_get(const vmfs_fs_t *fs,const vmfs_dirent_t *rec,u_char *buf)
{
   uint32_t blk_id = rec->block_id;

   if (VMFS_BLK_TYPE(blk_id) != VMFS_BLK_TYPE_FD)
      return(-1);

   return(vmfs_bitmap_get_item(fs->fdc, VMFS_BLK_FD_ENTRY(blk_id),
                               VMFS_BLK_FD_ITEM(blk_id), buf) ? 0 : -1);
}

/* Bind inode info to a file */
int vmfs_inode_bind(vmfs_file_t *f,const u_char *inode_buf)
{
   uint32_t blk_id = 0, blk_count;
   uint64_t blk_size;
   int i;

   vmfs_inode_read(&f->inode,inode_buf);

   /* We don't have a block list for RDM files */
   if (f->inode.type == VMFS_FILE_TYPE_RDM)
      return(0);

   blk_size = f->inode.blk_size;
   blk_count = (f->inode.size + blk_size - 1) / blk_size;
   vmfs_blk_list_init(&f->blk_list,blk_count);

   switch(f->inode.zla) {
   case VMFS_BLK_TYPE_FB:
   case VMFS_BLK_TYPE_SB:
      for(i=0;i<blk_count;i++) {
         blk_id = read_le32(inode_buf,VMFS_INODE_OFS_BLK_ARRAY+(i*sizeof(uint32_t)));
         vmfs_blk_list_add_block(&f->blk_list,i,blk_id);
      }
      break;
   case VMFS_BLK_TYPE_PB:
      {
         DECL_ALIGNED_BUFFER_WOL(buf,f->fs->pbc->bmh.data_size);
         uint32_t blk_per_pb = f->fs->pbc->bmh.data_size / sizeof(uint32_t);
         uint32_t pb_num, blk_in_pb;
         for(i=0,pb_num=0,blk_in_pb=0;i<blk_count;i++) {
            uint32_t blk_id2;
            if (blk_in_pb == 0) {
               blk_id = read_le32(inode_buf,
                          VMFS_INODE_OFS_BLK_ARRAY+(pb_num*sizeof(uint32_t)));
               if (blk_id == 0) {
                  pb_num++;
                  continue;
               }
               if (!vmfs_bitmap_get_item(f->fs->pbc,VMFS_BLK_PB_ENTRY(blk_id),
                                      VMFS_BLK_PB_ITEM(blk_id),buf))
                  goto error;
            }
            blk_id2 = read_le32(buf,blk_in_pb*sizeof(uint32_t));
            vmfs_blk_list_add_block(&f->blk_list,i,blk_id2);
            if (++blk_in_pb == blk_per_pb) {
               blk_in_pb = 0;
               pb_num++;
            }
         }
         break;
      }
   default:
      fprintf(stderr, "vmfs_inode_bind: "
              "unexpected zla 0x%2.2x!\n", f->inode.zla);
      goto error;
   }

   return(0);
error:
   vmfs_blk_list_free(&f->blk_list);
   return(-1);
}

/* Get inode status */
int vmfs_inode_stat(const vmfs_inode_t *inode,struct stat *buf)
{
   memset(buf,0,sizeof(*buf));
   buf->st_mode  = inode->cmode;
   buf->st_nlink = inode->nlink;
   buf->st_uid   = inode->uid;
   buf->st_gid   = inode->gid;
   buf->st_size  = inode->size;
   buf->st_atime = inode->atime;
   buf->st_mtime = inode->mtime;
   buf->st_ctime = inode->ctime;
   return(0);
}
