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
#ifndef VMFS_LVM_H
#define VMFS_LVM_H

#define VMFS_LVM_MAX_EXTENTS 32

struct vmfs_lvminfo {
   uuid_t uuid;
   uint32_t num_extents;
   uint64_t size;
   uint64_t blocks;
};

/* === LVM === */
struct vmfs_lvm {
   vmfs_flags_t flags;

   /* LVM information */
   vmfs_lvminfo_t lvm_info;

   /* number of extents currently loaded in the lvm */
   int loaded_extents;

   /* extents */
   vmfs_volume_t *extents[VMFS_LVM_MAX_EXTENTS];
};

/* Read a raw block of data on logical volume */
ssize_t vmfs_lvm_read(const vmfs_lvm_t *lvm,off_t pos,u_char *buf,size_t len);

/* Write a raw block of data on logical volume */
ssize_t vmfs_lvm_write(const vmfs_lvm_t *lvm,off_t pos,const u_char *buf,size_t len);

/* Reserve the underlying volume given a LVM position */
int vmfs_lvm_reserve(const vmfs_lvm_t *lvm,off_t pos);

/* Release the underlying volume given a LVM position */
int vmfs_lvm_release(const vmfs_lvm_t *lvm,off_t pos);

/* Show lvm information */
void vmfs_lvm_show(const vmfs_lvm_t *lvm);

/* Create a volume structure */
vmfs_lvm_t *vmfs_lvm_create(vmfs_flags_t flags);

/* Add an extent to the LVM */
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, vmfs_volume_t *vol);

/* Open an LVM */
int vmfs_lvm_open(vmfs_lvm_t *lvm);

/* Close an LVM */
void vmfs_lvm_close(vmfs_lvm_t *lvm);

#endif
