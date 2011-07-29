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
 * VMFS LVM layer
 */

#include <stdlib.h>
#include "vmfs.h"

/* 
 * Until we uncover the details of the segment descriptors format,
 * it is useless to try to do something more efficient.
 */

static vmfs_volume_t *vmfs_lvm_get_extent_from_offset(const vmfs_lvm_t *lvm,
                                                      off_t pos)
{
   int extent;
   off_t segment = pos / VMFS_LVM_SEGMENT_SIZE;

   for (extent = 0; extent < lvm->loaded_extents; extent++) {
      if ((segment >= lvm->extents[extent]->vol_info.first_segment) &&
          (segment <= lvm->extents[extent]->vol_info.last_segment))
        return(lvm->extents[extent]);
   }

   return(NULL);
}

/* Get extent size */
static inline uint64_t vmfs_lvm_extent_size(const vmfs_volume_t *extent)
{
   return((uint64_t)extent->vol_info.num_segments * VMFS_LVM_SEGMENT_SIZE);
}

typedef ssize_t (*vmfs_vol_io_func)(const vmfs_device_t *,off_t,u_char *,size_t);

/* Read a raw block of data on logical volume */
static inline ssize_t vmfs_lvm_io(const vmfs_lvm_t *lvm,off_t pos,u_char *buf,
                                  size_t len,vmfs_vol_io_func func)
{
   vmfs_volume_t *extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (!extent)
      return(-1);

   pos -= (uint64_t)extent->vol_info.first_segment * VMFS_LVM_SEGMENT_SIZE;
   if ((pos + len) > vmfs_lvm_extent_size(extent)) {
      /* TODO: Handle this case */
      fprintf(stderr,"VMFS: i/o spanned over several extents is unsupported\n");
      return(-1);
   }

   return(func(&extent->dev,pos,buf,len));
}

/* Read a raw block of data on logical volume */
static ssize_t vmfs_lvm_read(const vmfs_device_t *dev,off_t pos,
                             u_char *buf,size_t len)
{
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)dev;
   return(vmfs_lvm_io(lvm,pos,buf,len,vmfs_device_read));
}

/* Write a raw block of data on logical volume */
static ssize_t vmfs_lvm_write(const vmfs_device_t *dev,off_t pos,
                              const u_char *buf,size_t len)
{
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)dev;
   return(vmfs_lvm_io(lvm,pos,(u_char *)buf,len,(vmfs_vol_io_func)vmfs_device_write));
}

/* Reserve the underlying volume given a LVM position */
static int vmfs_lvm_reserve(const vmfs_device_t *dev,off_t pos)
{
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)dev;
   vmfs_volume_t *extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (!extent)
      return(-1);

   return(vmfs_device_reserve(&extent->dev, 0));
}

/* Release the underlying volume given a LVM position */
static int vmfs_lvm_release(const vmfs_device_t *dev,off_t pos)
{
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)dev;
   vmfs_volume_t *extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (!extent)
      return(-1);

   return(vmfs_device_release(&extent->dev, 0));
}

/* Create a volume structure */
vmfs_lvm_t *vmfs_lvm_create(vmfs_flags_t flags)
{
   vmfs_lvm_t *lvm;

   if (!(lvm = calloc(1,sizeof(*lvm))))
      return NULL;

   lvm->flags = flags;

   if (flags.read_write)
      fprintf(stderr, "VMFS: R/W support is experimental. Use at your own risk\n");

   return lvm;
}

/* Add an extent to the LVM */
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, vmfs_volume_t *vol)
{
   uint32_t i;

   if (!vol)
      return(-1);

   if (lvm->loaded_extents == 0) {
      uuid_copy(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid);
      lvm->lvm_info.size = vol->vol_info.lvm_size;
      lvm->lvm_info.blocks = vol->vol_info.blocks;
      lvm->lvm_info.num_extents = vol->vol_info.num_extents;
   } else if (uuid_compare(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid)) {
      fprintf(stderr, "VMFS: The %s file/device is not part of the LVM\n", vol->device);
      return(-1);
   } else if ((lvm->lvm_info.size != vol->vol_info.lvm_size) ||
              (lvm->lvm_info.blocks != vol->vol_info.blocks) ||
              (lvm->lvm_info.num_extents != vol->vol_info.num_extents)) {
      fprintf(stderr, "VMFS: LVM information mismatch for the %s"
                      " file/device\n", vol->device);
      return(-1);
   }

   for (i = 0;
        (i < lvm->loaded_extents) &&
           (vol->vol_info.first_segment > lvm->extents[i]->vol_info.first_segment);
        i++);

   if (lvm->loaded_extents)
      memmove(&lvm->extents[i + 1], &lvm->extents[i],
              (lvm->loaded_extents - i) * sizeof(vmfs_volume_t *));
   lvm->extents[i] = vol;
   lvm->loaded_extents++;

   return(0);
}

/* Close an LVM */
static void vmfs_lvm_close(vmfs_device_t *dev)
{
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)dev;
   if (!lvm)
      return;
   while(lvm->loaded_extents--)
      vmfs_device_close(&lvm->extents[lvm->loaded_extents]->dev);

   free(lvm);
}

/* Open an LVM */
int vmfs_lvm_open(vmfs_lvm_t *lvm)
{
   if (!lvm->flags.allow_missing_extents && 
       (lvm->loaded_extents != lvm->lvm_info.num_extents)) 
   {
      fprintf(stderr, "VMFS: Missing extents\n");
      return(-1);
   }

   lvm->dev.read = vmfs_lvm_read;
   if (lvm->flags.read_write)
      lvm->dev.write = vmfs_lvm_write;
   lvm->dev.reserve = vmfs_lvm_reserve;
   lvm->dev.release = vmfs_lvm_release;
   lvm->dev.close = vmfs_lvm_close;
   lvm->dev.uuid = &lvm->lvm_info.uuid;
   return(0);
}
