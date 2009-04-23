#ifndef VMFS_FS_H
#define VMFS_FS_H

/* === FS Info === */
#define VMFS_FSINFO_BASE   0x0200000
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

/* === VMFS filesystem === */
struct vmfs_fs {
   int debug_level;

   /* FDC base */
   off_t fdc_base;

   /* FS information */
   vmfs_fsinfo_t fs_info;

   /* Associated VMFS Volume */
   vmfs_volume_t *vol;

   /* Meta-files containing file system structures */
   vmfs_file_t *fbb,*fdc,*pbc,*sbc,*vh,*root_dir;

   /* Bitmap headers of meta-files */
   vmfs_bitmap_header_t fbb_bmh,fdc_bmh,pbc_bmh,sbc_bmh;
};

/* Get block size of a volume */
static inline m_u64_t vmfs_fs_get_blocksize(vmfs_fs_t *fs)
{
   return(fs->fs_info.block_size);
}

/* Read a block from the filesystem */
ssize_t vmfs_fs_read(vmfs_fs_t *fs,m_u32_t blk,off_t offset,
                      u_char *buf,size_t len);

/* Show FS information */
void vmfs_fsinfo_show(vmfs_fsinfo_t *fsi);

/* Dump volume bitmaps */
int vmfs_fs_dump_bitmaps(vmfs_fs_t *vol);

/* Create a FS structure */
vmfs_fs_t *vmfs_fs_create(char *filename,int debug_level);

/* Open a VMFS volume */
int vmfs_fs_open(vmfs_fs_t *fs);

#endif
