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
#ifndef VMFS_HEARTBEAT_H
#define VMFS_HEARTBEAT_H

#include <stddef.h>

#define VMFS_HB_BASE  0x0300000

#define VMFS_HB_SIZE  0x200

#define VMFS_HB_NUM   2048

#define VMFS_HB_MAGIC_OFF   0xabcdef01
#define VMFS_HB_MAGIC_ON    0xabcdef02

struct vmfs_heartbeart_raw {
   m_u32_t magic;
   m_u64_t pos;
   m_u64_t seq;
   m_u64_t uptime;
   uuid_t uuid;
   m_u32_t journal_block;
} __attribute__((packed));

#define VMFS_HB_OFS_MAGIC        offsetof(struct vmfs_heartbeart_raw, magic)
#define VMFS_HB_OFS_POS          offsetof(struct vmfs_heartbeart_raw, pos)
#define VMFS_HB_OFS_SEQ          offsetof(struct vmfs_heartbeart_raw, seq)
#define VMFS_HB_OFS_UPTIME       offsetof(struct vmfs_heartbeart_raw, uptime)
#define VMFS_HB_OFS_UUID         offsetof(struct vmfs_heartbeart_raw, uuid)
#define VMFS_HB_OFS_JOURNAL_BLK  offsetof(struct vmfs_heartbeart_raw, journal_block)

struct vmfs_heartbeat {
   m_u32_t magic;
   m_u64_t position;
   m_u64_t seq;          /* Sequence number */
   m_u64_t uptime;       /* Uptime (in usec) of the locker */
   uuid_t uuid;          /* UUID of the server */
   m_u32_t journal_blk;  /* Journal block */
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
