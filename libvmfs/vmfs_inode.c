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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <assert.h>
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
static int vmfs_inode_read(vmfs_inode_t *inode,const u_char *buf)
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
   } else if (inode->zla == VMFS5_ZLA_BASE + VMFS_BLK_TYPE_FD) {
      memcpy(inode->content, buf + VMFS_INODE_OFS_CONTENT, inode->size);
   } else {
      for(i=0;i<VMFS_INODE_BLK_COUNT;i++)
         inode->blocks[i] = vmfs_inode_read_blk_id(buf,i);
   }

   return(0);
}

/* Write an inode */
static int vmfs_inode_write(const vmfs_inode_t *inode,u_char *buf)
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
int vmfs_inode_update(const vmfs_inode_t *inode,int update_blk_list)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_INODE_SIZE);

   memset(buf,0,VMFS_INODE_SIZE);
   vmfs_inode_write(inode,buf);

   if (update_blk_list) {
      vmfs_inode_write_blk_list(inode,buf);
   } else {
      buf_len -= VMFS_INODE_BLK_COUNT * sizeof(uint32_t);
   }

   if (vmfs_device_write(inode->fs->dev,inode->mdh.pos,buf,buf_len) != buf_len)
      return(-1);

   return(0);
}

/* Get inode corresponding to a block id */
int vmfs_inode_get(const vmfs_fs_t *fs,uint32_t blk_id,vmfs_inode_t *inode)
{
   DECL_ALIGNED_BUFFER_WOL(buf,VMFS_INODE_SIZE);

   if (VMFS_BLK_TYPE(blk_id) != VMFS_BLK_TYPE_FD)
      return(-1);

   if (!vmfs_bitmap_get_item(fs->fdc, VMFS_BLK_FD_ENTRY(blk_id),
                             VMFS_BLK_FD_ITEM(blk_id), buf))
      return(-1);

   return(vmfs_inode_read(inode,buf));
}

/* Hash function to retrieve an in-core inode */
static inline u_int vmfs_inode_hash(const vmfs_fs_t *fs,uint32_t blk_id)
{
   return( (blk_id ^ (blk_id >> 9)) & (fs->inode_hash_buckets - 1) );
}

/* Register an inode in the in-core inode hash table */
static void vmfs_inode_register(const vmfs_fs_t *fs,vmfs_inode_t *inode)
{
   u_int hb;

   hb = vmfs_inode_hash(fs,inode->id);

   inode->fs = fs;
   inode->ref_count = 1;
   
   /* Insert into hash table */
   inode->next  = fs->inodes[hb];
   inode->pprev = &fs->inodes[hb];

   if (inode->next != NULL)
      inode->next->pprev = &inode->next;

   fs->inodes[hb] = inode;
}

/* Acquire an inode */
vmfs_inode_t *vmfs_inode_acquire(const vmfs_fs_t *fs,uint32_t blk_id)
{
   vmfs_inode_t *inode;
   u_int hb;

   hb = vmfs_inode_hash(fs,blk_id);
   for(inode=fs->inodes[hb];inode;inode=inode->next)
      if (inode->id == blk_id) {
         inode->ref_count++;
         return inode;
      }
   
   /* Inode not yet used, allocate room for it */
   if (!(inode = calloc(1,sizeof(*inode))))
      return NULL;

   if (vmfs_inode_get(fs,blk_id,inode) == -1) {
      free(inode);
      return NULL;
   }

   vmfs_inode_register(fs,inode);
   return inode;
}

/* Release an inode */
void vmfs_inode_release(vmfs_inode_t *inode)
{
   assert(inode->ref_count > 0);
 
   if (--inode->ref_count == 0) {
      if (inode->update_flags)
         vmfs_inode_update(inode,inode->update_flags & VMFS_INODE_SYNC_BLK);

      if (inode->pprev != NULL) {
         /* remove the inode from hash table */
         if (inode->next != NULL)
            inode->next->pprev = inode->pprev;

         *(inode->pprev) = inode->next;

         free(inode);
      }
   }
}

/* Allocate a new inode */
int vmfs_inode_alloc(vmfs_fs_t *fs,u_int type,mode_t mode,vmfs_inode_t **inode)
{
   vmfs_inode_t *fdc_inode;
   off_t fdc_offset;
   uint32_t fdc_blk;
   time_t ct;

   time(&ct);

   if (!(*inode = calloc(1,sizeof(vmfs_inode_t))))
      return(-ENOMEM);

   (*inode)->mdh.magic = VMFS_INODE_MAGIC;
   (*inode)->type      = type;
   (*inode)->blk_size  = fs->sbc->bmh.data_size;
   (*inode)->zla       = VMFS_BLK_TYPE_SB;
   (*inode)->mtime     = ct;
   (*inode)->ctime     = ct;
   (*inode)->atime     = ct;
   (*inode)->id2       = ++fs->inode_gen;
   (*inode)->mode      = mode;
   (*inode)->cmode     = (*inode)->mode | vmfs_file_type2mode((*inode)->type);

   if ((vmfs_block_alloc(fs,VMFS_BLK_TYPE_FD,&(*inode)->id)) < 0) {
      free(*inode);
      return(-ENOSPC);
   }

   /* Compute "physical" position of inode, using FDC file */
   fdc_inode = fs->fdc->f->inode;

   fdc_offset = vmfs_bitmap_get_item_pos(fs->fdc,
                                         VMFS_BLK_FD_ENTRY((*inode)->id),
                                         VMFS_BLK_FD_ITEM((*inode)->id));

   if ((vmfs_inode_get_block(fdc_inode,fdc_offset,&fdc_blk) == -1) ||
       (VMFS_BLK_TYPE(fdc_blk) != VMFS_BLK_TYPE_FB))
   {
      vmfs_block_free(fs,(*inode)->id);
      free(*inode);
      return(-ENOSPC);
   }

   (*inode)->mdh.pos = fdc_inode->blk_size * VMFS_BLK_FB_ITEM(fdc_blk);
   (*inode)->mdh.pos += fdc_offset % fdc_inode->blk_size;

   (*inode)->update_flags |= VMFS_INODE_SYNC_ALL;
   vmfs_inode_register(fs,*inode);
   return(0);
}

/* 
 * Get block ID corresponding the specified position. Pointer block
 * resolution is transparently done here.
 */
int vmfs_inode_get_block(const vmfs_inode_t *inode,off_t pos,uint32_t *blk_id)
{
   const vmfs_fs_t *fs = inode->fs;
   u_int blk_index;
   uint32_t zla;
   int vmfs5_extension;

   *blk_id = 0;

   if (!inode->blk_size)
      return(-EIO);

   /* This doesn't make much sense but looks like how it's being coded. At
    * least, the result has some sense. */
   zla = inode->zla;
   if (zla >= VMFS5_ZLA_BASE) {
      vmfs5_extension = 1;
      zla -= VMFS5_ZLA_BASE;
   } else
      vmfs5_extension = 0;

   switch(zla) {
      case VMFS_BLK_TYPE_FB:
      case VMFS_BLK_TYPE_SB:
         blk_index = pos / inode->blk_size;
         
         if (blk_index >= VMFS_INODE_BLK_COUNT)
            return(-EINVAL);

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
            return(-EINVAL);

         pb_blk_id = inode->blocks[pb_index];

         if (!pb_blk_id)
            break;

         if (!vmfs_bitmap_get_item(fs->pbc,
                                   VMFS_BLK_PB_ENTRY(pb_blk_id),
                                   VMFS_BLK_PB_ITEM(pb_blk_id),
                                   buf))
            return(-EIO);

         *blk_id = read_le32(buf,sub_index*sizeof(uint32_t));
         break;
      }

      case VMFS_BLK_TYPE_FD:
         if (vmfs5_extension) {
            *blk_id = inode->id;
            break;
         }
      default:
         /* Unexpected ZLA type */
         return(-EIO);
   }

   return(0);
}

/* Aggregate a sub-block to a file block */
static int vmfs_inode_aggregate_fb(vmfs_inode_t *inode)
{
   const vmfs_fs_t *fs = inode->fs;
   DECL_ALIGNED_BUFFER(buf,fs->sbc->bmh.data_size);
   uint32_t fb_blk,sb_blk,fb_item;
   uint32_t sb_count;
   off_t pos;
   int i,res;

   sb_count = vmfs_fs_get_blocksize(fs) / buf_len;

   if (!(buf = iobuffer_alloc(buf_len)))
      return(-ENOMEM);

   sb_blk = inode->blocks[0];

   if (!vmfs_bitmap_get_item(fs->sbc,
                             VMFS_BLK_SB_ENTRY(sb_blk),
                             VMFS_BLK_SB_ITEM(sb_blk),
                             buf)) 
   {
      res = -EIO;
      goto err_sb_blk_read;
   }

   if ((res = vmfs_block_alloc(fs,VMFS_BLK_TYPE_FB,&fb_blk)) < 0)
      goto err_blk_alloc;

   fb_item = VMFS_BLK_FB_ITEM(fb_blk);

   if (vmfs_fs_write(fs,fb_item,0,buf,buf_len) != buf_len) {
      res = -EIO;
      goto err_fs_write;
   }

   memset(buf,0,buf_len);
   pos = buf_len;

   for(i=1;i<sb_count;i++) {
      if (vmfs_fs_write(fs,fb_item,pos,buf,buf_len) != buf_len) {
         res = -EIO;
         goto err_fs_write;
      }

      pos += buf_len;
   }

   inode->blocks[0] = fb_blk;
   inode->zla = VMFS_BLK_TYPE_FB;
   inode->blk_size = vmfs_fs_get_blocksize(fs);
   inode->update_flags |= VMFS_INODE_SYNC_BLK;

   iobuffer_free(buf);
   return(0);

 err_fs_write:
   vmfs_block_free(fs,fb_blk);
 err_sb_blk_read:
 err_blk_alloc:
   iobuffer_free(buf);
   return(res);
}

/* Aggregate block list of an inode to a pointer block */
static int vmfs_inode_aggregate_pb(vmfs_inode_t *inode)
{
   const vmfs_fs_t *fs = inode->fs;
   uint32_t pb_blk,pb_len;
   uint32_t item,entry;
   u_char *buf;
   int i,res;

   pb_len = fs->pbc->bmh.data_size;

   if (pb_len < (VMFS_INODE_BLK_COUNT * sizeof(uint32_t))) {
      fprintf(stderr,"vmfs_inode_aggregate_pb: pb_len=0x%8.8x\n",pb_len);
      return(-EIO);
   }

   if (!(buf = iobuffer_alloc(pb_len)))
      return(-ENOMEM);

   memset(buf,0,pb_len);

   if ((res = vmfs_block_alloc(fs,VMFS_BLK_TYPE_PB,&pb_blk)) < 0)
      goto err_blk_alloc;

   for(i=0;i<VMFS_INODE_BLK_COUNT;i++)
      write_le32(buf,i*sizeof(uint32_t),inode->blocks[i]);

   entry = VMFS_BLK_PB_ENTRY(pb_blk);
   item  = VMFS_BLK_PB_ITEM(pb_blk);

   if (vmfs_bitmap_set_item(fs->pbc,entry,item,buf) == -1) {
      res = -EIO;
      goto err_set_item;
   }

   memset(inode->blocks,0,sizeof(inode->blocks));
   inode->blocks[0] = pb_blk;
   inode->zla = VMFS_BLK_TYPE_PB;
   inode->update_flags |= VMFS_INODE_SYNC_BLK;

   iobuffer_free(buf);
   return(0);

 err_set_item:
   vmfs_block_free(fs,pb_blk);
 err_blk_alloc:
   iobuffer_free(buf);
   return(res);
}

/* Proceed to block aggregation if the specified offset */
static int vmfs_inode_aggregate(vmfs_inode_t *inode,off_t pos)
{
   int res;

   if ((inode->zla == VMFS_BLK_TYPE_SB) && (pos >= inode->blk_size))
   {
      /* A directory consists only of sub-blocks (except the root dir) */
      if (inode->type == VMFS_FILE_TYPE_DIR)
         return(-EFBIG);

      if ((res = vmfs_inode_aggregate_fb(inode)) < 0)
         return(res);
   }

   if ((inode->zla == VMFS_BLK_TYPE_FB) &&
       (pos >= (inode->blk_size * VMFS_INODE_BLK_COUNT)))
      return(vmfs_inode_aggregate_pb(inode));

   return(0);
}

/* Get a block for writing corresponding to the specified position */
int vmfs_inode_get_wrblock(vmfs_inode_t *inode,off_t pos,uint32_t *blk_id)
{
   const vmfs_fs_t *fs = inode->fs;
   u_int blk_index;
   int res;

   if (!vmfs_fs_readwrite(fs))
      return(-EROFS);

   *blk_id = 0;

   if ((res = vmfs_inode_aggregate(inode,pos)) < 0)
      return(res);

   if (inode->zla == VMFS_BLK_TYPE_PB) {
      DECL_ALIGNED_BUFFER_WOL(buf,fs->pbc->bmh.data_size);
      uint32_t pb_blk_id;
      uint32_t blk_per_pb;
      u_int pb_index;
      u_int sub_index;
      bool update_pb;
      
      update_pb = 0;

      blk_per_pb = fs->pbc->bmh.data_size / sizeof(uint32_t);
      blk_index = pos / inode->blk_size;

      pb_index  = blk_index / blk_per_pb;
      sub_index = blk_index % blk_per_pb;

      if (pb_index >= VMFS_INODE_BLK_COUNT)
         return(-EINVAL);

      pb_blk_id = inode->blocks[pb_index];

      /* Allocate a Pointer Block if none is currently present */
      if (!pb_blk_id) {
         if ((res = vmfs_block_alloc(fs,VMFS_BLK_TYPE_PB,&pb_blk_id)) < 0)
            return(res);

         memset(buf,0,fs->pbc->bmh.data_size);
         inode->blocks[pb_index] = pb_blk_id;
         inode->update_flags |= VMFS_INODE_SYNC_BLK;
         update_pb = 1;
      } else {
         if (!vmfs_bitmap_get_item(fs->pbc,
                                   VMFS_BLK_PB_ENTRY(pb_blk_id),
                                   VMFS_BLK_PB_ITEM(pb_blk_id),
                                   buf))
            return(-EIO);

         *blk_id = read_le32(buf,sub_index*sizeof(uint32_t));
      }

      if (!*blk_id) {
         if ((res = vmfs_block_alloc(fs,VMFS_BLK_TYPE_FB,blk_id)) < 0)
            return(res);

         write_le32(buf,sub_index*sizeof(uint32_t),*blk_id);
         inode->blk_count++;
         inode->update_flags |= VMFS_INODE_SYNC_BLK;
         update_pb = 1;
      } else {
         if (VMFS_BLK_FB_TBZ(*blk_id)) {
            if ((res = vmfs_block_zeroize_fb(fs,*blk_id)) < 0)
               return(res);

            *blk_id = VMFS_BLK_FB_TBZ_CLEAR(*blk_id);
            write_le32(buf,sub_index*sizeof(uint32_t),*blk_id);
            inode->tbz--;
            inode->update_flags |= VMFS_INODE_SYNC_BLK;
            update_pb = 1;
         }
      }

      /* Update the pointer block on disk if it has been modified */
      if (update_pb && !vmfs_bitmap_set_item(fs->pbc,
                                             VMFS_BLK_PB_ENTRY(pb_blk_id),
                                             VMFS_BLK_PB_ITEM(pb_blk_id),
                                             buf))
         return(-EIO);
   } else {
      /* File Block or Sub-Block */
      blk_index = pos / inode->blk_size;
         
      if (blk_index >= VMFS_INODE_BLK_COUNT)
         return(-EINVAL);

      *blk_id = inode->blocks[blk_index];

      if (!*blk_id) {
         if ((res = vmfs_block_alloc(fs,inode->zla,blk_id)) < 0)
            return(res);

         inode->blocks[blk_index] = *blk_id;
         inode->blk_count++;
         inode->update_flags |= VMFS_INODE_SYNC_BLK;
      } else {
         if ((inode->zla == VMFS_BLK_TYPE_FB) && VMFS_BLK_FB_TBZ(*blk_id)) {
            if ((res = vmfs_block_zeroize_fb(fs,*blk_id)) < 0)
               return(res);

            *blk_id = VMFS_BLK_FB_TBZ_CLEAR(*blk_id);
            inode->blocks[blk_index] = *blk_id;
            inode->tbz--;
            inode->update_flags |= VMFS_INODE_SYNC_BLK;
         }
      }
   }

   return(0);
}

/* Truncate file */
int vmfs_inode_truncate(vmfs_inode_t *inode,off_t new_len)
{
   const vmfs_fs_t *fs = inode->fs;
   u_int i;
   int res;

   if (!vmfs_fs_readwrite(fs))
      return(-EROFS);

   if (new_len == inode->size)
      return(0);

   if (new_len > inode->size) {
      if ((res = vmfs_inode_aggregate(inode,new_len)) < 0)
         return(res);

      inode->size = new_len;
      inode->update_flags |= VMFS_INODE_SYNC_META;
      return(0);
   }

   switch(inode->zla) {
      case VMFS_BLK_TYPE_FB:
      case VMFS_BLK_TYPE_SB:
      {
         u_int start,end;

         start = ALIGN_NUM(new_len,inode->blk_size) / inode->blk_size;
         end   = inode->size / inode->blk_size;

         for(i=start;i<=end;i++) {
            if (inode->blocks[i] != 0) {
               vmfs_block_free(fs,inode->blocks[i]);
               inode->blk_count--;
               inode->blocks[i] = 0;
            }
         }
         break;
      }

      case VMFS_BLK_TYPE_PB:
      {
         uint32_t blk_per_pb;
         u_int pb_start,pb_end;
         u_int sub_start,start;
         u_int blk_index;
         int count;

         blk_per_pb = fs->pbc->bmh.data_size / sizeof(uint32_t);
         blk_index = ALIGN_NUM(new_len,inode->blk_size) / inode->blk_size;

         pb_start  = blk_index / blk_per_pb;
         sub_start = blk_index % blk_per_pb;

         pb_end = inode->size / (inode->blk_size * blk_per_pb);

         for(i=pb_start;i<=pb_end;i++) {
            if (inode->blocks[i] != 0) {
               start = (i == pb_start) ? sub_start : 0;

               /* Free blocks contained in PB */
               count = vmfs_block_free_pb(fs,inode->blocks[i],
                                          start,blk_per_pb);

               if (count > 0)
                  inode->blk_count -= count;

               if (start == 0)
                  inode->blocks[i] = 0;
            }
         }

         break;
      }

      default:
         return(-EIO);
   }

   inode->size = new_len;
   inode->update_flags |= VMFS_INODE_SYNC_BLK;
   return(0);
}

/* Call a function for each allocated block of an inode */
int vmfs_inode_foreach_block(const vmfs_inode_t *inode,
                             vmfs_inode_foreach_block_cbk_t cbk,
                             void *opt_arg)
{  
   const vmfs_fs_t *fs = inode->fs;
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

   if (blk_count > VMFS_INODE_BLK_COUNT)
      return(-1);

   for(i=0;i<blk_count;i++) {
      blk_id = inode->blocks[i];

      if (!blk_id)
         continue;

      cbk(inode,0,blk_id,opt_arg);

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

            cbk(inode,blk_id,blk_id2,opt_arg);
         }
      }
   }

   return(0);
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

/* Get inode status */
int vmfs_inode_stat_from_blkid(const vmfs_fs_t *fs,uint32_t blk_id,
                               struct stat *buf)
{
   vmfs_inode_t *inode;

   if (!(inode = vmfs_inode_acquire(fs,blk_id)))
      return(-EIO);

   vmfs_inode_stat(inode,buf);
   vmfs_inode_release(inode);
   return(0);
}

/* Change permissions */
int vmfs_inode_chmod(vmfs_inode_t *inode,mode_t mode)
{
   inode->mode = mode;
   inode->update_flags |= VMFS_INODE_SYNC_META;
   return(0);
}
