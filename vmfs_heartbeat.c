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
 * VMFS heartbeats.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include "utils.h"
#include "vmfs.h"

/* Read a heartbeart info */
int vmfs_heartbeat_read(vmfs_heartbeat_t *hb,const u_char *buf)
{
   hb->magic       = read_le32(buf,VMFS_HB_OFS_MAGIC);
   hb->position    = read_le64(buf,VMFS_HB_OFS_POS);
   hb->seq         = read_le64(buf,VMFS_HB_OFS_SEQ);
   hb->uptime      = read_le64(buf,VMFS_HB_OFS_UPTIME);
   hb->journal_blk = read_le32(buf,VMFS_HB_OFS_JOURNAL_BLK);
   read_uuid(buf,VMFS_HB_OFS_UUID,&hb->uuid);
   return(0);
}

/* Write a heartbeat info */
int vmfs_heartbeat_write(const vmfs_heartbeat_t *hb,u_char *buf)
{
   write_le32(buf,VMFS_HB_OFS_MAGIC,hb->magic);
   write_le64(buf,VMFS_HB_OFS_POS,hb->position);
   write_le64(buf,VMFS_HB_OFS_SEQ,hb->seq);
   write_le64(buf,VMFS_HB_OFS_UPTIME,hb->uptime);
   write_le32(buf,VMFS_HB_OFS_JOURNAL_BLK,hb->journal_blk);
   write_uuid(buf,VMFS_HB_OFS_UUID,&hb->uuid);
   return(0);
}

/* Show heartbeat info */
void vmfs_heartbeat_show(const vmfs_heartbeat_t *hb)
{
   char uuid_str[M_UUID_BUFLEN];
   
   printf("Heartbeat ID 0x%"PRIx64":\n",hb->position);

   printf("  - Magic    : 0x%8.8x\n",hb->magic);
   printf("  - Sequence : 0x%8.8"PRIx64"\n",hb->seq);
   printf("  - Uptime   : 0x%8.8"PRIx64"\n",hb->uptime);
   printf("  - UUID     : %s\n",m_uuid_to_str(hb->uuid,uuid_str));

   printf("\n");
}

/* Show the active locks */
int vmfs_heartbeat_show_active(const vmfs_fs_t *fs)
{
   u_char buf[VMFS_HB_SIZE];
   vmfs_heartbeat_t hb;
   ssize_t res;
   off_t pos = 0;
   int count = 0;

   while(pos < VMFS_HB_SIZE * VMFS_HB_NUM) {
      res = vmfs_lvm_read(fs->lvm,VMFS_HB_BASE+pos,buf,sizeof(buf));

      if (res != sizeof(buf)) {
         fprintf(stderr,"VMFS: unable to read heartbeat info.\n");
         return(-1);
      }

      vmfs_heartbeat_read(&hb,buf);
      
      if (hb.magic == VMFS_HB_MAGIC_ON) {
         vmfs_heartbeat_show(&hb);
         count++;
      } else if (hb.magic != VMFS_HB_MAGIC_OFF) {
         fprintf(stderr,"VMFS: unable to read heartbeat info.\n");
         break;
      }

      pos += res;
   }
   
   return(count);
}
