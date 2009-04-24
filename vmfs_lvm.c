/*
 * VMFS LVM layer
 */

#include <stdlib.h>
#include "vmfs.h"

#define VMFS_LVM_SEGMENT_SIZE (256 * 1024 * 1024)

/* Until we uncover the details of the segment descriptors format,
   it is useless to try to do something more efficient */

static int vmfs_lvm_get_extent_from_offset(vmfs_lvm_t *lvm,off_t pos)
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

/* Read a raw block of data on logical volume */
ssize_t vmfs_lvm_read(vmfs_lvm_t *lvm,off_t pos,u_char *buf,size_t len)
{
   int extent = vmfs_lvm_get_extent_from_offset(lvm,pos);
   if (extent < 0)
      return(-1);
   pos -= lvm->extents[extent]->vol_info.first_segment * VMFS_LVM_SEGMENT_SIZE;
   if (pos + len > lvm->extents[extent]->vol_info.num_segments * VMFS_LVM_SEGMENT_SIZE) {
      /* TODO: Handle this case */
      fprintf(stderr,"VMFS: i/o spanned over several extents is unsupported\n");
      return(-1);
   }

   return(vmfs_vol_read(lvm->extents[extent],pos,buf,len));
}

/* Create a volume structure */
vmfs_lvm_t *vmfs_lvm_create(int debug_level)
{
   vmfs_lvm_t *lvm;

   if (!(lvm = calloc(1,sizeof(*lvm))))
      return NULL;

   lvm->debug_level = debug_level;
   return lvm;
}

/* Add an extent to the LVM */
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, char *filename)
{
   vmfs_volume_t *vol;

   if (!(vol = vmfs_vol_create(filename, lvm->debug_level)))
      return(-1);

   if (vmfs_vol_open(vol))
      return(-1);

   if (lvm->loaded_extents == 0) {
      uuid_copy(lvm->lvm_info.uuid, vol->vol_info.lvm_uuid);
      lvm->lvm_info.size = vol->vol_info.size;
      lvm->lvm_info.blocks = vol->vol_info.blocks;
      lvm->lvm_info.num_extents = vol->vol_info.num_extents;
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
