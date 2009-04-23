#ifndef VMFS_VOLUME_H
#define VMFS_VOLUME_H

/* === Volume Info === */
#define VMFS_VOLINFO_BASE   0x100000
#define VMFS_VOLINFO_MAGIC  0xc001d00d

#define VMFS_VOLINFO_OFS_MAGIC 0x0000
#define VMFS_VOLINFO_OFS_VER   0x0004
#define VMFS_VOLINFO_OFS_NAME  0x0012
#define VMFS_VOLINFO_OFS_UUID  0x0082 
/* 0x0092 64-bits timestamp in usec (volume ctime?) */
/* 0x009a 64-bits timestamp in usec (volume mtime?) */

#define VMFS_VOLINFO_OFS_NAME_SIZE     28

/* === LVM Info === */
#define VMFS_LVMINFO_OFS_SIZE          0x0200
#define VMFS_LVMINFO_OFS_BLKS          0x0208 /* Seems to be systematically sum(VMFS_LVMINFO_OFS_NUM_SEGMENTS for all extents) + VMFS_LVMINFO_OFS_NUM_EXTENTS */
#define VMFS_LVMINFO_OFS_UUID_STR      0x0214
#define VMFS_LVMINFO_OFS_UUID          0x0254
/* 0x0268 64-bits timestamp in usec (lvm ctime?) */
#define VMFS_LVMINFO_OFS_NUM_SEGMENTS  0x0274
#define VMFS_LVMINFO_OFS_FIRST_SEGMENT 0x0278
#define VMFS_LVMINFO_OFS_LAST_SEGMENT  0x0280
/* 0x0288 64-bits timestamp in usec (lvm mtime?) */
#define VMFS_LVMINFO_OFS_NUM_EXTENTS   0x0290

/* Segment information are at 0x80600 + i * 0x80 for i between 0 and VMFS_LVMINFO_OFS_NUM_SEGMENTS */
/* At 0x10 (64-bits) or 0x14 (32-bits) within a segment info, it seems like something related to the absolute segment number in the logical volume (looks like absolute segment number << 4 on 32-bits) */
/* Other segment information seem relative to the extent (always the same pattern on all extents) */

struct vmfs_volinfo {
   m_u32_t magic;
   m_u32_t version;
   char *name;
   uuid_t uuid;

   m_u64_t size;
   m_u64_t blocks;
   uuid_t lvm_uuid;
   m_u32_t num_segments,
           first_segment,
           last_segment,
           num_extents;
};

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

/* === VMFS mounted-volume === */
struct vmfs_volume {
   char *filename;
   FILE *fd;
   int debug_level;

   /* VMFS volume base */
   off_t vmfs_base;

   /* FDC base */
   off_t fdc_base;

   /* Volume and FS information */
   vmfs_volinfo_t vol_info;
   vmfs_fsinfo_t fs_info;

   /* Meta-files containing file system structures */
   vmfs_file_t *fbb,*fdc,*pbc,*sbc,*vh,*root_dir;

   /* Bitmap headers of meta-files */
   vmfs_bitmap_header_t fbb_bmh,fdc_bmh,pbc_bmh,sbc_bmh;
};

/* Get block size of a volume */
static inline m_u64_t vmfs_vol_get_blocksize(vmfs_volume_t *vol)
{
   return(vol->fs_info.block_size);
}

/* Read a raw block of data */
ssize_t vmfs_vol_read_data(vmfs_volume_t *vol,off_t pos,u_char *buf,size_t len);

/* Read data given a block position */
ssize_t vmfs_vol_read(vmfs_volume_t *vol,m_u32_t blk,off_t offset,
                      u_char *buf,size_t len);

/* Read volume information */
int vmfs_volinfo_read(vmfs_volinfo_t *vol,FILE *fd);

/* Show volume information */
void vmfs_volinfo_show(vmfs_volinfo_t *vol);

/* Read filesystem information */
int vmfs_fsinfo_read(vmfs_fsinfo_t *fsi,FILE *fd,off_t base);

/* Show FS information */
void vmfs_fsinfo_show(vmfs_fsinfo_t *fsi);

/* Create a volume structure */
vmfs_volume_t *vmfs_vol_create(char *filename,int debug_level);

/* Dump volume bitmaps */
int vmfs_vol_dump_bitmaps(vmfs_volume_t *vol);

/* Open a VMFS volume */
int vmfs_vol_open(vmfs_volume_t *vol);

#endif
