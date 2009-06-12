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

static inline uint32_t vmfs_inode_read_blk_id(const u_char *buf,u_int index)
{
   return(read_le32(buf,VMFS_INODE_OFS_BLK_ARRAY+(index*sizeof(uint32_t))));
}

static inline void vmfs_inode_write_blk_id(u_char *buf,u_int index,
                                           uint32_t blk_id)
{
   write_le32(buf,VMFS_INODE_OFS_BLK_ARRAY+(index*sizeof(uint32_t)),blk_id);
}

/* Read an inode */
int vmfs_inode_read(vmfs_inode_t *inode,const u_char *buf)
{
   int i;

   vmfs_metadata_hdr_read(&inode->mdh,buf);

   if (inode->mdh.magic != VMFS_INODE_MAGIC)
      return(-1);

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
   } else {
      for(i=0;i<VMFS_INODE_BLK_COUNT;i++)
         inode->blocks[i] = vmfs_inode_read_blk_id(buf,i);
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

/* Update block list */
static void vmfs_inode_write_blk_list(const vmfs_inode_t *inode,u_char *buf)
{
   int i;

   for(i=0;i<VMFS_INODE_BLK_COUNT;i++)
      vmfs_inode_write_blk_id(buf,i,inode->blocks[i]);
}

/* Update an inode on disk */
int vmfs_inode_update(const vmfs_fs_t *fs,const vmfs_inode_t *inode,
                      int update_blk_list)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_INODE_SIZE);

   vmfs_inode_write(inode,buf);

   if (update_blk_list) {
      vmfs_inode_write_blk_list(inode,buf);
   } else {
      buf_len -= VMFS_INODE_BLK_COUNT * sizeof(uint32_t);
   }

   if (vmfs_lvm_write(fs->lvm,inode->mdh.pos,buf,buf_len) != buf_len)
      return(-1);

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

/* 
 * Get block ID corresponding the specified position. Pointer block
 * resolution is transparently done here.
 */
int vmfs_inode_get_block(const vmfs_fs_t *fs,const vmfs_inode_t *inode,
                         off_t pos,uint32_t *blk_id)
{
   u_int blk_index;

   *blk_id = 0;

   if (!inode->blk_size)
      return(-1);

   switch(inode->zla) {
      case VMFS_BLK_TYPE_FB:
      case VMFS_BLK_TYPE_SB:
         blk_index = pos / inode->blk_size;
         
         if (blk_index >= VMFS_INODE_BLK_COUNT)
            return(-1);

         *blk_id = inode->blocks[blk_index];
         break;

      case VMFS_BLK_TYPE_PB:
      {
         DECL_ALIGNED_BUFFER_WOL(buf,fs->pbc->bmh.data_size);
         uint32_t pb_blk_id;
         uint32_t blk_per_pb;
         u_int pb_index;
         u_int sub_index;

         blk_per_pb = fs->pbc->bmh.data_size / sizeof(uint32_t);
         blk_index = pos / inode->blk_size;

         pb_index  = blk_index / blk_per_pb;
         sub_index = blk_index % blk_per_pb;

         if (pb_index >= VMFS_INODE_BLK_COUNT)
            return(-1);

         pb_blk_id = inode->blocks[pb_index];

         if (!pb_blk_id)
            break;

         if (!vmfs_bitmap_get_item(fs->pbc,
                                   VMFS_BLK_PB_ENTRY(pb_blk_id),
                                   VMFS_BLK_PB_ITEM(pb_blk_id),
                                   buf))
            return(-1);

         *blk_id = read_le32(buf,sub_index*sizeof(uint32_t));
         break;
      }

      default:
         /* Unexpected ZLA type */
         return(-1);
   }

   return(0);
}

/* Show block list of an inode */
void vmfs_inode_show_blocks(const vmfs_fs_t *fs,const vmfs_inode_t *inode)
{
   uint64_t blk_size;
   uint32_t blk_per_pb;
   u_int blk_count;
   int i;

   blk_size = inode->blk_size;
   
   if (!blk_size)
      return;

   blk_count = (inode->size + blk_size - 1) / blk_size;

   if (inode->zla == VMFS_BLK_TYPE_PB) {
      blk_per_pb = fs->pbc->bmh.data_size / sizeof(uint32_t);
      blk_count = (blk_count + blk_per_pb - 1) / blk_per_pb;
   }

   for(i=0;i<blk_count;i++) {
      if (i && !(i % 4)) printf("\n");
      printf("0x%8.8x ",inode->blocks[i]);
   }

   printf("\n");
}

/* Call a function for each allocated block of an inode */
int vmfs_inode_foreach_block(const vmfs_fs_t *fs,const vmfs_inode_t *inode,
                             vmfs_inode_foreach_block_cbk_t cbk,void *opt_arg)
{         
   uint64_t blk_size;
   uint32_t blk_per_pb;
   uint32_t blk_id;
   u_int blk_total;
   u_int blk_count;
   int i,j,err_count;

   err_count = 0;
   blk_total = 0;
   blk_per_pb = 0;

   blk_size = inode->blk_size;

   if (!blk_size)
      return(-1);

   blk_count = (inode->size + blk_size - 1) / blk_size;

   if (inode->zla == VMFS_BLK_TYPE_PB) {
      blk_per_pb = fs->pbc->bmh.data_size / sizeof(uint32_t);
      blk_total = blk_count;
      blk_count = (blk_count + blk_per_pb - 1) / blk_per_pb;
   }

   for(i=0;i<blk_count;i++) {
      blk_id = inode->blocks[i];

      if (!blk_id)
         continue;

      cbk(fs,inode,0,blk_id,opt_arg);

      /* Analyze pointer block */
      if (inode->zla == VMFS_BLK_TYPE_PB) 
      {
         DECL_ALIGNED_BUFFER_WOL(buf,fs->pbc->bmh.data_size);
         uint32_t blk_id2;
         u_int blk_rem;

         if (!vmfs_bitmap_get_item(fs->pbc,
                                   VMFS_BLK_PB_ENTRY(blk_id),
                                   VMFS_BLK_PB_ITEM(blk_id),
                                   buf))
            return(-1);

         /* Compute remaining blocks */
         blk_rem = m_min(blk_total - (i * blk_per_pb),blk_per_pb);

         for(j=0;j<blk_rem;j++) {
            blk_id2 = read_le32(buf,j*sizeof(uint32_t));

            if (!blk_id2)
               continue;

            cbk(fs,inode,blk_id,blk_id2,opt_arg);
         }
      }
   }

   return(0);
}

/* Check that an inode block is allocated */
static void vmfs_inode_check_block_alloc(const vmfs_fs_t *fs,
                                         const vmfs_inode_t *inode,
                                         uint32_t pb_blk,
                                         uint32_t blk_id,
                                         void *opt_arg)
{
   int *err_count = opt_arg;

   if (vmfs_block_get_status(fs,blk_id) <= 0) {
      if (!pb_blk)
         fprintf(stderr,"Block 0x%8.8x is not allocated\n",blk_id);
      else
         fprintf(stderr,"Block 0x%8.8x in PB 0x%8.8x is not allocated\n",
                 blk_id,pb_blk);

      (*err_count)++;
   }
}

/* Check that all blocks bound to an inode are allocated */
int vmfs_inode_check_blocks(const vmfs_fs_t *fs,const vmfs_inode_t *inode)
{
   int status,err_count = 0;

   status = vmfs_inode_foreach_block(fs,inode,vmfs_inode_check_block_alloc,
                                     &err_count);

   if (status == -1)
      return(-1);

   return(err_count);
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
