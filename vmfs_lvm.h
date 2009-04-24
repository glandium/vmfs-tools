#ifndef VMFS_LVM_H
#define VMFS_LVM_H

#define VMFS_LVM_MAX_EXTENTS 32

struct vmfs_lvminfo {
   uuid_t uuid;
   m_u32_t num_extents;
   m_u64_t size;
   m_u64_t blocks;
};

/* === LVM === */
struct vmfs_lvm {
   int debug_level;

   /* LVM information */
   vmfs_lvminfo_t lvm_info;

   /* number of extents currently loaded in the lvm */
   int loaded_extents;

   /* extents */
   vmfs_volume_t *extents[VMFS_LVM_MAX_EXTENTS];
};

/* Read a raw block of data on logical volume */
ssize_t vmfs_lvm_read(vmfs_lvm_t *lvm,off_t pos,u_char *buf,size_t len);

/* Create a volume structure */
vmfs_lvm_t *vmfs_lvm_create(int debug_level);

/* Add an extent to the LVM */
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, char *filename);

/* Open an LVM */
int vmfs_lvm_open(vmfs_lvm_t *lvm);

#endif
