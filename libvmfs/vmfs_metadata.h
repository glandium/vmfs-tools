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
#ifndef VMFS_METADATA_H
#define VMFS_METADATA_H

#include <stddef.h>

#define VMFS_METADATA_HDR_SIZE  512

struct vmfs_metadata_hdr_raw {
   uint32_t magic;         /* Magic number */
   uint64_t pos;           /* Position in the volume */
   uint64_t hb_pos;        /* Heartbeat position */
   uint64_t hb_seq;        /* Heartbeat sequence */
   uint64_t obj_seq;       /* Object sequence */
   uint32_t hb_lock;       /* Heartbeat lock flag */
   uuid_t hb_uuid;         /* UUID of locking server */
   uint64_t mtime;
   u_char pad1[0x1c0];     /* Padding/unknown */
} __attribute__((packed));

#define VMFS_MDH_OFS_MAGIC    offsetof(struct vmfs_metadata_hdr_raw, magic)
#define VMFS_MDH_OFS_POS      offsetof(struct vmfs_metadata_hdr_raw, pos)
#define VMFS_MDH_OFS_HB_POS   offsetof(struct vmfs_metadata_hdr_raw, hb_pos)
#define VMFS_MDH_OFS_HB_SEQ   offsetof(struct vmfs_metadata_hdr_raw, hb_seq)
#define VMFS_MDH_OFS_OBJ_SEQ  offsetof(struct vmfs_metadata_hdr_raw, obj_seq)
#define VMFS_MDH_OFS_HB_LOCK  offsetof(struct vmfs_metadata_hdr_raw, hb_lock)
#define VMFS_MDH_OFS_HB_UUID  offsetof(struct vmfs_metadata_hdr_raw, hb_uuid)
#define VMFS_MDH_OFS_MTIME    offsetof(struct vmfs_metadata_hdr_raw, mtime)

struct vmfs_metadata_hdr {
   uint32_t magic;
   uint64_t pos;
   uint64_t hb_pos;
   uint64_t hb_seq;
   uint64_t obj_seq;
   uint32_t hb_lock;
   uuid_t  hb_uuid;
   uint64_t mtime;
};

static inline bool vmfs_metadata_is_locked(vmfs_metadata_hdr_t *mdh)
{
   return(mdh->hb_lock != 0);
}

/* Read a metadata header */
int vmfs_metadata_hdr_read(vmfs_metadata_hdr_t *mdh,const u_char *buf);

/* Write a metadata header */
int vmfs_metadata_hdr_write(const vmfs_metadata_hdr_t *mdh,u_char *buf);

/* Lock and read metadata at specified position */
int vmfs_metadata_lock(vmfs_fs_t *fs,off_t pos,u_char *buf,size_t buf_len,
                       vmfs_metadata_hdr_t *mdh);

/* Unlock metadata */
int vmfs_metadata_unlock(vmfs_fs_t *fs,vmfs_metadata_hdr_t *mdh);

#endif
