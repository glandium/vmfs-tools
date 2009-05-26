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
 * VMFS metadata headers.
 */

#include <string.h>
#include <sys/stat.h>
#include "vmfs.h"

/* Read a metadata header */
int vmfs_metadata_hdr_read(vmfs_metadata_hdr_t *mdh,const u_char *buf)
{
   mdh->magic     = read_le32(buf,VMFS_MDH_OFS_MAGIC);
   mdh->position  = read_le64(buf,VMFS_MDH_OFS_POS);
   mdh->hb_pos    = read_le64(buf,VMFS_MDH_OFS_HB_POS);
   mdh->hb_seq    = read_le64(buf,VMFS_MDH_OFS_HB_SEQ);
   mdh->obj_seq   = read_le64(buf,VMFS_MDH_OFS_OBJ_SEQ);
   mdh->hb_lock   = read_le32(buf,VMFS_MDH_OFS_HB_LOCK);
   mdh->mtime     = read_le64(buf,VMFS_MDH_OFS_MTIME);
   read_uuid(buf,VMFS_MDH_OFS_HB_UUID,&mdh->hb_uuid);
   return(0);
}

/* Write a metadata header */
int vmfs_metadata_hdr_write(const vmfs_metadata_hdr_t *mdh,u_char *buf)
{
   write_le32(buf,VMFS_MDH_OFS_MAGIC,mdh->magic);
   write_le64(buf,VMFS_MDH_OFS_POS,mdh->position);
   write_le64(buf,VMFS_MDH_OFS_HB_POS,mdh->hb_pos);
   write_le64(buf,VMFS_MDH_OFS_HB_SEQ,mdh->hb_seq);
   write_le64(buf,VMFS_MDH_OFS_OBJ_SEQ,mdh->obj_seq);
   write_le32(buf,VMFS_MDH_OFS_HB_LOCK,mdh->hb_lock);
   write_le64(buf,VMFS_MDH_OFS_MTIME,mdh->mtime);
   write_uuid(buf,VMFS_MDH_OFS_HB_UUID,&mdh->hb_uuid);
   return(0);
}

/* Show a metadata header */
void vmfs_metadata_hdr_show(const vmfs_metadata_hdr_t *mdh)
{
   printf("  - Magic        : 0x%8.8x\n",mdh->magic);
   printf("  - Position     : 0x%"PRIx64"\n",mdh->position);
   printf("  - HB Position  : 0x%"PRIx64"\n",mdh->hb_pos);
   printf("  - HB Lock      : %d (%s)\n",
          mdh->hb_lock,(mdh->hb_lock > 0) ? "LOCKED":"UNLOCKED");
   printf("  - HB Sequence  : 0x%"PRIx64"\n",mdh->hb_seq);
   printf("  - Obj Sequence : 0x%"PRIx64"\n",mdh->obj_seq);
   printf("  - MTime        : %"PRId64"\n",mdh->mtime);
}

/* Lock and read metadata at specified position */
int vmfs_metadata_lock(vmfs_fs_t *fs,off_t pos,u_char *buf,size_t buf_len,
                       vmfs_metadata_hdr_t *mdh)
{
   int res = -1;

   /* We must have an active heartbeat to lock metadata */
   if (!vmfs_heartbeat_active(&fs->hb))
      return(-1);

   /* Reserve volume */
   if (vmfs_lvm_reserve(fs->lvm,pos) == -1) {
      fprintf(stderr,"VMFS: unable to reserve volume.\n");
      return(-1);
   }

   /* Read the complete metadata for the caller */
   if (vmfs_lvm_read(fs->lvm,pos,buf,buf_len) != buf_len) {
      fprintf(stderr,"VMFS: unable to read metadata.\n");
      goto done;
   }

   vmfs_metadata_hdr_read(mdh,buf);
   
   if (mdh->hb_lock != 0)
      goto done;

   /* Update metadata information */
   mdh->obj_seq++;
   mdh->hb_lock = 1;
   mdh->hb_pos  = fs->hb.pos;
   mdh->hb_seq  = fs->hb_seq;
   uuid_copy(mdh->hb_uuid,fs->hb.uuid);
   vmfs_metadata_hdr_write(mdh,buf);

   /* Rewrite the metadata header only */
   if (vmfs_lvm_write(fs->lvm,pos,buf,VMFS_METADATA_HDR_SIZE) 
       != VMFS_METADATA_HDR_SIZE)
   {
      fprintf(stderr,"VMFS: unable to write metadata header.\n");
      goto done;
   }

   res = 0;
 done:
   vmfs_lvm_release(fs->lvm,pos);
   return(res);
}

/* Unlock metadata */
int vmfs_metadata_unlock(vmfs_fs_t *fs,off_t pos,u_char *buf,size_t buf_len,
                         vmfs_metadata_hdr_t *mdh)
{
   if (buf_len < VMFS_METADATA_HDR_SIZE)
      return(-1);

   mdh->hb_lock = 0;
   uuid_clear(mdh->hb_uuid);
   vmfs_metadata_hdr_write(mdh,buf);

   /* Rewrite the metadata header only */
   if (vmfs_lvm_write(fs->lvm,pos,buf,VMFS_METADATA_HDR_SIZE) 
       != VMFS_METADATA_HDR_SIZE) 
   {
      fprintf(stderr,"VMFS: unable to write metadata header.\n");
      return(-1);
   }

   return(0);
}
