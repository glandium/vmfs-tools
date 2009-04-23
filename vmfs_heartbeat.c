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
int vmfs_heartbeat_read(vmfs_heartbeat_t *hb,u_char *buf)
{
   hb->magic    = read_le32(buf,VMFS_HB_OFS_MAGIC);
   hb->position = read_le64(buf,VMFS_HB_OFS_POS);
   hb->uptime   = read_le64(buf,VMFS_HB_OFS_UPTIME);
   memcpy(hb->uuid,buf+VMFS_HB_OFS_UUID,sizeof(hb->uuid));

   return(0);
}

/* Show heartbeat info */
void vmfs_heartbeat_show(vmfs_heartbeat_t *hb)
{
   char uuid_str[M_UUID_BUFLEN];
   
   printf("Heartbeat ID 0x%llx:\n",hb->position);

   printf("  - Magic  : 0x%8.8x\n",hb->magic);
   printf("  - Uptime : 0x%8.8llx\n",hb->uptime);
   printf("  - UUID   : %s\n",m_uuid_to_str(hb->uuid,uuid_str));

   printf("\n");
}

/* Show the active locks */
int vmfs_heartbeat_show_active(vmfs_fs_t *fs)
{
   u_char buf[VMFS_HB_SIZE];
   vmfs_heartbeat_t hb;
   ssize_t res;
   off_t pos = 0;
   int count = 0;

   while(pos < vmfs_fs_get_blocksize(fs)) {
      res = vmfs_vol_read(fs->vol,3,pos,buf,sizeof(buf));

      if (res != sizeof(buf)) {
         fprintf(stderr,"VMFS: unable to read heartbeat info.\n");
         return(-1);
      }

      vmfs_heartbeat_read(&hb,buf);
      
      if (hb.magic == VMFS_HB_MAGIC_ON) {
         vmfs_heartbeat_show(&hb);
         count++;
      }

      pos += res;
   }
   
   return(count);
}
