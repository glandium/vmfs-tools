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
   hb->pos         = read_le64(buf,VMFS_HB_OFS_POS);
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
   write_le64(buf,VMFS_HB_OFS_POS,hb->pos);
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
   
   printf("Heartbeat ID 0x%"PRIx64":\n",hb->pos);

   printf("  - Magic    : 0x%8.8x\n",hb->magic);
   printf("  - Sequence : 0x%8.8"PRIx64"\n",hb->seq);
   printf("  - Uptime   : 0x%8.8"PRIx64"\n",hb->uptime);
   printf("  - UUID     : %s\n",m_uuid_to_str(hb->uuid,uuid_str));

   printf("\n");
}

/* Show the active locks */
int vmfs_heartbeat_show_active(const vmfs_fs_t *fs)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_HB_SIZE);
   vmfs_heartbeat_t hb;
   ssize_t res;
   off_t pos = 0;
   int count = 0;

   while(pos < VMFS_HB_SIZE * VMFS_HB_NUM) {
      res = vmfs_lvm_read(fs->lvm,VMFS_HB_BASE+pos,buf,buf_len);

      if (res != buf_len) {
         fprintf(stderr,"VMFS: unable to read heartbeat info.\n");
         return(-1);
      }

      vmfs_heartbeat_read(&hb,buf);
      
      if (vmfs_heartbeat_active(&hb)) {
         vmfs_heartbeat_show(&hb);
         count++;
      } else if (hb.magic != VMFS_HB_MAGIC_OFF) {
         fprintf(stderr,"VMFS: invalid heartbeat info.\n");
         break;
      }

      pos += res;
   }
   
   return(count);
}

/* Lock an heartbeat given its ID */
int vmfs_heartbeat_lock(vmfs_fs_t *fs,u_int id,vmfs_heartbeat_t *hb)
{  
   DECL_ALIGNED_BUFFER(buf,VMFS_HB_SIZE);
   off_t pos;
   int res = -1;

   if (id >= VMFS_HB_NUM)
      return(-1);

   pos = VMFS_HB_BASE + (id * VMFS_HB_SIZE);
   
   if (vmfs_lvm_reserve(fs->lvm,pos) == -1) {
      fprintf(stderr,"VMFS: unable to reserve volume.\n");
      return(-1);
   }

   if (vmfs_lvm_read(fs->lvm,pos,buf,buf_len) != buf_len) {
      fprintf(stderr,"VMFS: unable to read heartbeat info.\n");
      goto done;
   }

   vmfs_heartbeat_read(hb,buf);
   
   if (vmfs_heartbeat_active(hb))
      goto done;

   hb->magic = VMFS_HB_MAGIC_ON;
   hb->uptime = vmfs_host_get_uptime();
   hb->seq++;
   vmfs_host_get_uuid(hb->uuid);
   vmfs_heartbeat_write(hb,buf);

   if (vmfs_lvm_write(fs->lvm,pos,buf,buf_len) != buf_len) {
      fprintf(stderr,"VMFS: unable to write heartbeat info.\n");
      hb->magic = VMFS_HB_MAGIC_OFF;
      goto done;
   }

   res = 0;
 done:
   vmfs_lvm_release(fs->lvm,pos);
   return(res);
}

/* Unlock an heartbeat */
int vmfs_heartbeat_unlock(vmfs_fs_t *fs,vmfs_heartbeat_t *hb)
{
   DECL_ALIGNED_BUFFER(buf,VMFS_HB_SIZE);

   if (!vmfs_heartbeat_active(hb))
      return(-1);

   hb->magic = VMFS_HB_MAGIC_OFF;
   hb->seq++;
   uuid_clear(hb->uuid);
   vmfs_heartbeat_write(hb,buf);

   return((vmfs_lvm_write(fs->lvm,hb->pos,buf,buf_len) == buf_len) ? 0 : -1);
}

/* Update an heartbeat */
int vmfs_heartbeat_update(vmfs_fs_t *fs,vmfs_heartbeat_t *hb)
{  
   DECL_ALIGNED_BUFFER(buf,VMFS_HB_SIZE);

   if (!vmfs_heartbeat_active(hb))
      return(-1);

   hb->uptime = vmfs_host_get_uptime();
   vmfs_heartbeat_write(hb,buf);

   return((vmfs_lvm_write(fs->lvm,hb->pos,buf,buf_len) == buf_len) ? 0 : -1);
}

/* Acquire an heartbeat (ID is chosen automatically) */
int vmfs_heartbeat_acquire(vmfs_fs_t *fs)
{
   vmfs_heartbeat_t hb;   
   u_char *buf;
   size_t buf_len;
   int i,res = -1;

   /* Heartbeat already active ? */
   if (fs->hb_refcount > 0) {
      fs->hb_refcount++;
      fs->hb_expire = vmfs_host_get_uptime() + VMFS_HEARTBEAT_EXPIRE_DELAY;
      return(0);
   }

   /* Try to reuse the current ID */
   if (!vmfs_heartbeat_lock(fs,fs->hb_id,&fs->hb))
      return(0);

   buf_len = VMFS_HB_NUM * VMFS_HB_SIZE;

   if (!(buf = iobuffer_alloc(buf_len)))
      return(-1);

   if (vmfs_lvm_read(fs->lvm,VMFS_HB_BASE,buf,buf_len) != buf_len)
      return(-1);

   /* 
    * Heartbeat is taken by someone else, find a new one.
    * To avoid high contention with SCSI reservation, we first read
    * directly the heartbeat info, and if the heartbeat is not taken, 
    * we try to acquire it definitely with reservation.
    */
   for(i=0;i<VMFS_HB_NUM;i++) {
      vmfs_heartbeat_read(&hb,buf+(i * VMFS_HB_SIZE));

      if (vmfs_heartbeat_active(&hb))
         continue;

      if (!vmfs_heartbeat_lock(fs,i,&fs->hb)) {
         fs->hb_id  = i;
         fs->hb_seq = fs->hb.seq;
         fs->hb_refcount = 1;
         fs->hb_expire = vmfs_host_get_uptime() + VMFS_HEARTBEAT_EXPIRE_DELAY;
         res = 0;
         break;
      }
   }

   iobuffer_free(buf);
   return(res);
}

/* Release an heartbeat */
int vmfs_heartbeat_release(vmfs_fs_t *fs)
{
   if (fs->hb_refcount == 0)
      return(-1);

   /* The heartbeat will be eventually released by the background process */
   fs->hb_refcount--;
   return(0);
}
