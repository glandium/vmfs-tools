#ifndef VMFS_HEARTBEAT_H
#define VMFS_HEARTBEAT_H

#define VMFS_HB_BASE  0x0300000

#define VMFS_HB_SIZE  0x200

#define VMFS_HB_NUM   2048

#define VMFS_HB_MAGIC_OFF   0xabcdef01
#define VMFS_HB_MAGIC_ON    0xabcdef02

#define VMFS_HB_OFS_MAGIC   0x0000
#define VMFS_HB_OFS_POS     0x0004
#define VMFS_HB_OFS_SEQ     0x000c
#define VMFS_HB_OFS_UPTIME  0x0014
#define VMFS_HB_OFS_UUID    0x001c

struct vmfs_heartbeat {
   m_u32_t magic;
   m_u64_t position;
   m_u64_t seq;          /* Sequence number */
   m_u64_t uptime;       /* Uptime (in usec) of the locker */
   uuid_t uuid;          /* UUID of the server */
};

/* Read a heartbeart info */
int vmfs_heartbeat_read(vmfs_heartbeat_t *hb,const u_char *buf);

/* Write a heartbeat info */
int vmfs_heartbeat_write(const vmfs_heartbeat_t *hb,u_char *buf);

/* Show heartbeat info */
void vmfs_heartbeat_show(const vmfs_heartbeat_t *hb);

/* Show the active locks */
int vmfs_heartbeat_show_active(const vmfs_fs_t *fs);

#endif
