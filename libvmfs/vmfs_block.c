/*
 * vmfs-tools - Tools to access VMFS filesystems
 * Copyright (C) 2009 Christophe Fillot <cf@utc.fr>
 * Copyright (C) 2009,2012 Mike Hommey <mh@glandium.org>
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
 * VMFS blocks.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "utils.h"
#include "vmfs.h"

/* Get bitmap info (bitmap type,entry and item) from a block ID */
int vmfs_block_get_info(uint32_t blk_id, vmfs_block_info_t *info)
{
   uint32_t blk_type;

   blk_type = VMFS_BLK_TYPE(blk_id);

   switch(blk_type) {
      /* File Block */
      case VMFS_BLK_TYPE_FB:
         info->entry = 0;
         info->item  = VMFS_BLK_FB_ITEM(blk_id);
         info->flags = VMFS_BLK_FB_FLAGS(blk_id);
         break;

      /* Sub-Block */
      case VMFS_BLK_TYPE_SB:
         info->entry = VMFS_BLK_SB_ENTRY(blk_id);
         info->item  = VMFS_BLK_SB_ITEM(blk_id);
         info->flags = VMFS_BLK_SB_FLAGS(blk_id);
         break;

      /* Pointer Block */
      case VMFS_BLK_TYPE_PB:
         info->entry = VMFS_BLK_PB_ENTRY(blk_id);
         info->item  = VMFS_BLK_PB_ITEM(blk_id);
         info->flags = VMFS_BLK_PB_FLAGS(blk_id);
         info->flags = 0;
         break;

      /* Inode */
      case VMFS_BLK_TYPE_FD:
         info->entry = VMFS_BLK_FD_ENTRY(blk_id);
         info->item  = VMFS_BLK_FD_ITEM(blk_id);
         info->flags = VMFS_BLK_FD_FLAGS(blk_id);
         info->flags = 0;
         break;

      default:
         return(-1);
   }

   info->type = blk_type;

   return(0);
}

/* Get block status (allocated or free) */
int vmfs_block_get_status(const vmfs_fs_t *fs,uint32_t blk_id)
{
   vmfs_bitmap_entry_t entry;
   vmfs_bitmap_t *bmp;
   vmfs_block_info_t info;

   if (vmfs_block_get_info(blk_id,&info) == -1)
      return(-1);

   if (!(bmp = vmfs_fs_get_bitmap(fs, info.type)))
      return(-1);

   if (vmfs_bitmap_get_entry(bmp,info.entry,info.item,&entry) == -1)
      return(-1);

   return(vmfs_bitmap_get_item_status(&bmp->bmh,&entry,info.entry,info.item));
}

/* Allocate or free the specified block */
static int vmfs_block_set_status(const vmfs_fs_t *fs,uint32_t blk_id,
                                 int status)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_BITMAP_ENTRY_SIZE);
   vmfs_bitmap_entry_t entry;
   vmfs_bitmap_t *bmp;
   vmfs_block_info_t info;

   if (vmfs_block_get_info(blk_id,&info) == -1)
      return(-1);

   if (!(bmp = vmfs_fs_get_bitmap(fs, info.type)))
      return(-1);

   if (vmfs_bitmap_get_entry(bmp,info.entry,info.item,&entry) == -1)
      return(-1);

   /* Lock the bitmap entry to ensure exclusive access */
   if (!vmfs_metadata_lock((vmfs_fs_t *)fs,entry.mdh.pos,
                           buf,buf_len,&entry.mdh) == -1)
      return(-1);

   /* Mark the item as allocated */
   if (vmfs_bitmap_set_item_status(&bmp->bmh,&entry,
                                   info.entry,info.item,status) == -1)
   {
      vmfs_metadata_unlock((vmfs_fs_t *)fs,&entry.mdh);
      return(-1);
   }

   /* Update entry and release lock */
   vmfs_bme_update(fs,&entry);
   vmfs_metadata_unlock((vmfs_fs_t *)fs,&entry.mdh);
   return(0);
}

/* Allocate the specified block */
int vmfs_block_alloc_specified(const vmfs_fs_t *fs,uint32_t blk_id)
{ 
   return(vmfs_block_set_status(fs,blk_id,1));
}

/* Free the specified block */
int vmfs_block_free(const vmfs_fs_t *fs,uint32_t blk_id)
{
   return(vmfs_block_set_status(fs,blk_id,0));
}

/* Allocate a single block */
int vmfs_block_alloc(const vmfs_fs_t *fs,uint32_t blk_type,uint32_t *blk_id)
{
   vmfs_bitmap_t *bmp;
   vmfs_bitmap_entry_t entry;
   uint32_t item,addr;

   if (!(bmp = vmfs_fs_get_bitmap(fs, blk_type)))
      return(-EINVAL);

   if (vmfs_bitmap_find_free_items(bmp,1,&entry) == -1)
      return(-ENOSPC);

   if (vmfs_bitmap_alloc_item(&entry,&item) == -1) {
      vmfs_metadata_unlock((vmfs_fs_t *)fs,&entry.mdh);
      return(-ENOSPC);
   }

   vmfs_bme_update(fs,&entry);
   vmfs_metadata_unlock((vmfs_fs_t *)fs,&entry.mdh);

   switch(blk_type) {
      case VMFS_BLK_TYPE_FB:
         addr = (entry.id * bmp->bmh.items_per_bitmap_entry) + item;
         *blk_id = VMFS_BLK_FB_BUILD(addr, 0);
         break;
      case VMFS_BLK_TYPE_SB:
         *blk_id = VMFS_BLK_SB_BUILD(entry.id, item, 0);
         break;
      case VMFS_BLK_TYPE_PB:
         *blk_id = VMFS_BLK_PB_BUILD(entry.id, item, 0);
         break;
      case VMFS_BLK_TYPE_FD:
         *blk_id = VMFS_BLK_FD_BUILD(entry.id, item, 0);
         break;
   }

   return(0);
}

/* Zeroize a file block */
int vmfs_block_zeroize_fb(const vmfs_fs_t *fs,uint32_t blk_id)
{
   DECL_ALIGNED_BUFFER(buf,M_DIO_BLK_SIZE);
   uint32_t blk_item;
   off_t pos,len;

   if (VMFS_BLK_TYPE(blk_id) != VMFS_BLK_TYPE_FB)
      return(-EINVAL);

   memset(buf,0,buf_len);
   blk_item = VMFS_BLK_FB_ITEM(blk_id);
   len = vmfs_fs_get_blocksize(fs);
   pos = 0;

   while(pos < len) {
      if (vmfs_fs_write(fs,blk_item,pos,buf,buf_len) != buf_len)
         return(-EIO);

      pos += buf_len;
   }

   return(0);
}

/* Free blocks hold by a pointer block */
int vmfs_block_free_pb(const vmfs_fs_t *fs,uint32_t pb_blk,                     
                       u_int start,u_int end)
{     
   DECL_ALIGNED_BUFFER(buf,fs->pbc->bmh.data_size);
   uint32_t pbc_entry,pbc_item;
   uint32_t blk_id;
   int i,count = 0;

   if (VMFS_BLK_TYPE(pb_blk) != VMFS_BLK_TYPE_PB)
      return(-EINVAL);

   pbc_entry = VMFS_BLK_PB_ENTRY(pb_blk);
   pbc_item  = VMFS_BLK_PB_ITEM(pb_blk);

   if (!vmfs_bitmap_get_item(fs->pbc,pbc_entry,pbc_item,buf))
      return(-EIO);

   for(i=start;i<end;i++) {
      blk_id = read_le32(buf,i*sizeof(uint32_t));

      if (blk_id != 0) {
         vmfs_block_free(fs,blk_id);
         write_le32(buf,i*sizeof(uint32_t),0);
         count++;
      }
   }

   if ((start == 0) && (end == (buf_len / sizeof(uint32_t))))
      vmfs_block_free(fs,pb_blk);
   else {
      if (!vmfs_bitmap_set_item(fs->pbc,pbc_entry,pbc_item,buf))
         return(-EIO);
   }

   return(count);
}

/* Read a piece of a sub-block */
ssize_t vmfs_block_read_sb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                           u_char *buf,size_t len)
{
   DECL_ALIGNED_BUFFER_WOL(tmpbuf,fs->sbc->bmh.data_size);
   uint32_t offset,sbc_entry,sbc_item;
   size_t clen;

   offset = pos % fs->sbc->bmh.data_size;
   clen   = m_min(fs->sbc->bmh.data_size - offset,len);

   sbc_entry = VMFS_BLK_SB_ENTRY(blk_id);
   sbc_item  = VMFS_BLK_SB_ITEM(blk_id);

   if (!vmfs_bitmap_get_item(fs->sbc,sbc_entry,sbc_item,tmpbuf))
      return(-EIO);

   memcpy(buf,tmpbuf+offset,clen);
   return(clen);
}

/* Write a piece of a sub-block */
ssize_t vmfs_block_write_sb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                            u_char *buf,size_t len)
{
   DECL_ALIGNED_BUFFER_WOL(tmpbuf,fs->sbc->bmh.data_size);
   uint32_t offset,sbc_entry,sbc_item;
   size_t clen;

   offset = pos % fs->sbc->bmh.data_size;
   clen   = m_min(fs->sbc->bmh.data_size - offset,len);

   sbc_entry = VMFS_BLK_SB_ENTRY(blk_id);
   sbc_item  = VMFS_BLK_SB_ITEM(blk_id);

   /* If we write completely the sub-block, no need to read something */
   if (!offset && (clen == fs->sbc->bmh.data_size)) {
      if (!vmfs_bitmap_set_item(fs->sbc,sbc_entry,sbc_item,tmpbuf))
         return(-EIO);
      
      return(clen);
   }

   /* Read the full block and update a piece of it */
   if (!vmfs_bitmap_get_item(fs->sbc,sbc_entry,sbc_item,tmpbuf))
      return(-EIO);

   memcpy(tmpbuf+offset,buf,clen);

   if (!vmfs_bitmap_set_item(fs->sbc,sbc_entry,sbc_item,tmpbuf))
      return(-EIO);

   return(clen);
}

/* Read a piece of a file block */
ssize_t vmfs_block_read_fb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                           u_char *buf,size_t len)
{
   uint64_t offset,n_offset,blk_size;
   size_t clen,n_clen;
   uint32_t fb_item;
   u_char *tmpbuf;

   blk_size = vmfs_fs_get_blocksize(fs);

   offset = pos % blk_size;
   clen   = m_min(blk_size - offset,len);

   /* Use "normalized" offset / length to access data (for direct I/O) */
   n_offset = offset & ~(M_DIO_BLK_SIZE - 1);
   n_clen   = ALIGN_NUM(clen + (offset - n_offset),M_DIO_BLK_SIZE);

   fb_item = VMFS_BLK_FB_ITEM(blk_id);

   /* If everything is aligned for direct I/O, store directly in user buffer */
   if ((n_offset == offset) && (n_clen == clen) &&
       ALIGN_CHECK((uintptr_t)buf,M_DIO_BLK_SIZE))
   {
      if (vmfs_fs_read(fs,fb_item,n_offset,buf,n_clen) != n_clen)
         return(-EIO);

      return(n_clen);
   }

   /* Allocate a temporary buffer and copy result to user buffer */
   if (!(tmpbuf = iobuffer_alloc(n_clen)))
      return(-1);

   if (vmfs_fs_read(fs,fb_item,n_offset,tmpbuf,n_clen) != n_clen) {
      iobuffer_free(tmpbuf);
      return(-EIO);
   }

   memcpy(buf,tmpbuf+(offset-n_offset),clen);

   iobuffer_free(tmpbuf);
   return(clen);
}

/* Write a piece of a file block */
ssize_t vmfs_block_write_fb(const vmfs_fs_t *fs,uint32_t blk_id,off_t pos,
                            u_char *buf,size_t len)
{
   uint64_t offset,n_offset,blk_size;
   size_t clen,n_clen;
   uint32_t fb_item;
   u_char *tmpbuf;

   blk_size = vmfs_fs_get_blocksize(fs);

   offset = pos % blk_size;
   clen   = m_min(blk_size - offset,len);

   /* Use "normalized" offset / length to access data (for direct I/O) */
   n_offset = offset & ~(M_DIO_BLK_SIZE - 1);
   n_clen   = ALIGN_NUM(clen + (offset - n_offset),M_DIO_BLK_SIZE);

   fb_item = VMFS_BLK_FB_ITEM(blk_id);

   /* 
    * If everything is aligned for direct I/O, write directly from user 
    * buffer.
    */
   if ((n_offset == offset) && (n_clen == clen) &&
       ALIGN_CHECK((uintptr_t)buf,M_DIO_BLK_SIZE))
   {
      if (vmfs_fs_write(fs,fb_item,n_offset,buf,n_clen) != n_clen)
         return(-EIO);

      return(n_clen);
   }

   /* Allocate a temporary buffer */
   if (!(tmpbuf = iobuffer_alloc(n_clen)))
      return(-1);

   /* Read the original block and add user data */
   if (vmfs_fs_read(fs,fb_item,n_offset,tmpbuf,n_clen) != n_clen)
      goto err_io;
      
   memcpy(tmpbuf+(offset-n_offset),buf,clen);

   /* Write the modified block */
   if (vmfs_fs_write(fs,fb_item,n_offset,tmpbuf,n_clen) != n_clen)
      goto err_io;

   iobuffer_free(tmpbuf);
   return(clen);

 err_io:
   iobuffer_free(tmpbuf);
   return(-EIO);
}
