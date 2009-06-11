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

#define VMFS_LVM_SEGMENT_SIZE (256 * 1024 * 1024)

/* 
 * Until we uncover the details of the segment descriptors format,
 * it is useless to try to do something more efficient.
 */

static int vmfs_lvm_get_extent_from_offset(const vmfs_lvm_t *lvm,off_t pos)
{
   int extent;
   off_t segment = pos / VMFS_LVM_SEGMENT_SIZE;

   for (extent = 0; extent < lvm->loaded_extents; extent++) {
      if ((segment >= lvm->extents[extent]->vol_info.first_segment) &&
          (segment <= lvm->extents[extent]->vol_info.last_segment))
        return(extent);
   }

   return(-1);
}

/* Get extent size */
static inline uint64_t vmfs_lvm_extent_size(const vmfs_lvm_t *lvm,int extent)
{
   return((uint64_t)lvm->extents[extent]->vol_info.num_segments * VMFS_LVM_SEGMENT_SIZE);
}

typedef ssize_t (*vmfs_vol_io_func)(const vmfs_volume_t *,off_t,u_char *,size_t);

/* Read a raw block of data on logical volume */
static inline ssize_t vmfs_lvm_io(const vmfs_lvm_t *lvm,off_t pos,u_char *buf,
                                  size_t len,vmfs_vol_io_func func)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (extent < 0)
      return(-1);

   pos -= (uint64_t)lvm->extents[extent]->vol_info.first_segment * VMFS_LVM_SEGMENT_SIZE;
   if ((pos + len) > vmfs_lvm_extent_size(lvm,extent)) {
      /* TODO: Handle this case */
      fprintf(stderr,"VMFS: i/o spanned over several extents is unsupported\n");
      return(-1);
   }

   return(func(lvm->extents[extent],pos,buf,len));
}

/* Read a raw block of data on logical volume */
ssize_t vmfs_lvm_read(const vmfs_lvm_t *lvm,off_t pos,u_char *buf,size_t len)
{
   return(vmfs_lvm_io(lvm,pos,buf,len,vmfs_vol_read));
}

/* Write a raw block of data on logical volume */
ssize_t vmfs_lvm_write(const vmfs_lvm_t *lvm,off_t pos,const u_char *buf,size_t len)
{
   return(vmfs_lvm_io(lvm,pos,(u_char *)buf,len,(vmfs_vol_io_func)vmfs_vol_write));
}

/* Reserve the underlying volume given a LVM position */
int vmfs_lvm_reserve(const vmfs_lvm_t *lvm,off_t pos)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (extent < 0)
      return(-1);

   return(vmfs_vol_reserve(lvm->extents[extent]));
}

/* Release the underlying volume given a LVM position */
int vmfs_lvm_release(const vmfs_lvm_t *lvm,off_t pos)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);

   if (extent < 0)
      return(-1);

   return(vmfs_vol_release(lvm->extents[extent]));
}

/* Show lvm information */
void vmfs_lvm_show(const vmfs_lvm_t *lvm) {
   char uuid_str[M_UUID_BUFLEN];
   int i;

   printf("Logical Volume Information:\n");
   printf("  - UUID    : %s\n",m_uuid_to_str(lvm->lvm_info.uuid,uuid_str));
   printf("  - Size    : %"PRIu64" GB\n",
          lvm->extents[0]->vol_info.size / (1024*1048576));
   printf("  - Blocks  : %"PRIu64"\n",lvm->extents[0]->vol_info.blocks);
   printf("  - Num. Extents : %u\n",lvm->extents[0]->vol_info.num_extents);

   printf("\n");

   for(i = 0; i < lvm->loaded_extents; i++) {
      vmfs_vol_show(lvm->extents[i]);
   }
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
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, const char *filename)
{
   vmfs_volume_t *vol;

   if (!(vol = vmfs_vol_create(filename, lvm->flags)))
      return(-1);

   if (vmfs_vol_open(vol))
      return(-1);

   if (lvm->loaded_extents == 0) {
      uuid_copy(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid);
      lvm->lvm_info.size = vol->vol_info.size;
      lvm->lvm_info.blocks = vol->vol_info.blocks;
      lvm->lvm_info.num_extents = vol->vol_info.num_extents;
   } else if (uuid_compare(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid)) {
      fprintf(stderr, "VMFS: The %s file/device is not part of the LVM\n", filename);
      return(-1);
   } else if ((lvm->lvm_info.size != vol->vol_info.size) ||
              (lvm->lvm_info.blocks != vol->vol_info.blocks) ||
              (lvm->lvm_info.num_extents != vol->vol_info.num_extents)) {
      fprintf(stderr, "VMFS: LVM information mismatch for the %s"
                      " file/device\n", filename);
      return(-1);
   }

   lvm->extents[lvm->loaded_extents++] = vol;
   return(0);
}

/* Open an LVM */
int vmfs_lvm_open(vmfs_lvm_t *lvm)
{
   if (lvm->loaded_extents != lvm->lvm_info.num_extents) {
      fprintf(stderr, "VMFS: Missing extents\n");
      return(-1);
   }
   return(0);
}

/* Close an LVM */
void vmfs_lvm_close(vmfs_lvm_t *lvm)
{
   if (!lvm)
      return;
   while(lvm->loaded_extents--)
      vmfs_vol_close(lvm->extents[lvm->loaded_extents]);

   free(lvm);
}
