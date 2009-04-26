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
vmfs_lvm_t *vmfs_lvm_create(int debug_level);

/* Add an extent to the LVM */
int vmfs_lvm_add_extent(vmfs_lvm_t *lvm, char *filename);

/* Open an LVM */
int vmfs_lvm_open(vmfs_lvm_t *lvm);

#endif
