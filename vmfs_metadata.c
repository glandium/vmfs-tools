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
}
