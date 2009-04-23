#ifndef VMFS_FS_H
#define VMFS_FS_H

/* === FS Info === */
#define VMFS_FSINFO_BASE   0x1200000
#define VMFS_FSINFO_MAGIC  0x2fabf15e

#define VMFS_FSINFO_OFS_MAGIC    0x0000
#define VMFS_FSINFO_OFS_VOLVER   0x0004
#define VMFS_FSINFO_OFS_VER      0x0008
#define VMFS_FSINFO_OFS_UUID     0x0009
#define VMFS_FSINFO_OFS_LABEL    0x001d
#define VMFS_FSINFO_OFS_BLKSIZE  0x00a1
/* 0x00a9 32-bits timestamp (ctime?) */
/* 0x00b1 LVM uuid */

struct vmfs_fsinfo {
   m_u32_t magic;
   m_u32_t vol_version;
   m_u32_t version;
   uuid_t uuid;
   char label[128];

   m_u64_t block_size;
   uuid_t vol_uuid;
};

/* Read filesystem information */
int vmfs_fsinfo_read(vmfs_fsinfo_t *fsi,FILE *fd,off_t base);

/* Show FS information */
void vmfs_fsinfo_show(vmfs_fsinfo_t *fsi);

/* Dump volume bitmaps */
int vmfs_vol_dump_bitmaps(vmfs_volume_t *vol);

/* Read FDC base information ; temporarily exported */
int vmfs_read_fdc_base(vmfs_volume_t *vol);

#endif
