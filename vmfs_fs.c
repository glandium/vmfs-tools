/* 
 * VMFS filesystem..
 */

#include <string.h>
#include <stdlib.h>
#include "vmfs.h"

/* VMFS meta-files */
#define VMFS_FBB_FILENAME  ".fbb.sf"
#define VMFS_FDC_FILENAME  ".fdc.sf"
#define VMFS_PBC_FILENAME  ".pbc.sf"
#define VMFS_SBC_FILENAME  ".sbc.sf"
#define VMFS_VH_FILENAME   ".vh.sf"

#define VMFS_FDC_BASE       0x1400000

/* Read filesystem information */
int vmfs_fsinfo_read(vmfs_fsinfo_t *fsi,FILE *fd,off_t base)
{
   u_char buf[512];

   if (fseek(fd,base+VMFS_FSINFO_BASE,SEEK_SET) != 0)
      return(-1);

   if (fread(buf,sizeof(buf),1,fd) != 1)
      return(-1);

   fsi->magic       = read_le32(buf,VMFS_FSINFO_OFS_MAGIC);

   if (fsi->magic != VMFS_FSINFO_MAGIC) {
      fprintf(stderr,"VMFS FSInfo: invalid magic number 0x%8.8x\n",fsi->magic);
      return(-1);
   }

   fsi->vol_version = read_le32(buf,VMFS_FSINFO_OFS_VOLVER);
   fsi->version     = buf[VMFS_FSINFO_OFS_VER];

   fsi->block_size  = read_le32(buf,VMFS_FSINFO_OFS_BLKSIZE);

   memcpy(fsi->uuid,buf+VMFS_FSINFO_OFS_UUID,sizeof(fsi->uuid));
   memcpy(fsi->label,buf+VMFS_FSINFO_OFS_LABEL,sizeof(fsi->label));

   return(0);
}

/* Show FS information */
void vmfs_fsinfo_show(vmfs_fsinfo_t *fsi)
{  
   char uuid_str[M_UUID_BUFLEN];

   printf("VMFS FS Information:\n");

   printf("  - Vol. Version : %d\n",fsi->vol_version);
   printf("  - Version      : %d\n",fsi->version);
   printf("  - Label        : %s\n",fsi->label);
   printf("  - UUID         : %s\n",m_uuid_to_str(fsi->uuid,uuid_str));
   printf("  - Block size   : %llu (0x%llx)\n",
          fsi->block_size,fsi->block_size);

   printf("\n");
}

/* Read the root directory given its inode */
static int vmfs_read_rootdir(vmfs_volume_t *vol,u_char *inode_buf)
{
   if (!(vol->root_dir = vmfs_file_create_struct(vol)))
      return(-1);

   if (vmfs_inode_bind(vol->root_dir,inode_buf) == -1) {
      fprintf(stderr,"VMFS: unable to bind inode to root directory\n");
      return(-1);
   }
   
   return(0);
}

/* Read bitmap header */
static int vmfs_read_bitmap_header(vmfs_file_t *f,vmfs_bitmap_header_t *bmh)
{
   u_char buf[512];

   vmfs_file_seek(f,0,SEEK_SET);

   if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
      return(-1);

   return(vmfs_bmh_read(bmh,buf));
}

/* Open a meta-file */
static vmfs_file_t *vmfs_open_meta_file(vmfs_volume_t *vol,char *name,
                                        vmfs_bitmap_header_t *bmh)
{
   u_char buf[VMFS_INODE_SIZE];
   vmfs_dirent_t rec;
   vmfs_file_t *f;
   off_t inode_addr;

   if (!(f = vmfs_file_create_struct(vol)))
      return NULL;

   /* Search the file name in root directory */
   if (vmfs_dirent_search(vol->root_dir,name,&rec) != 1)
      return NULL;
   
   /* Read the inode info */
   inode_addr = vmfs_inode_get_offset(vol,rec.block_id);
   inode_addr += vol->fdc_base;

   if (vmfs_vol_read_data(vol,inode_addr,buf,sizeof(buf)) != sizeof(buf))
      return NULL;

   /* Bind the associated inode */
   if (vmfs_inode_bind(f,buf) == -1)
      return NULL;

   /* Read the bitmap header */
   if ((bmh != NULL) && (vmfs_read_bitmap_header(f,bmh) == -1))
      return NULL;
   
   return f;
}

/* Open all the VMFS meta files */
static int vmfs_open_all_meta_files(vmfs_volume_t *vol)
{
   vol->fbb = vmfs_open_meta_file(vol,VMFS_FBB_FILENAME,&vol->fbb_bmh);
   vol->fdc = vmfs_open_meta_file(vol,VMFS_FDC_FILENAME,&vol->fdc_bmh);
   vol->pbc = vmfs_open_meta_file(vol,VMFS_PBC_FILENAME,&vol->pbc_bmh);
   vol->sbc = vmfs_open_meta_file(vol,VMFS_SBC_FILENAME,&vol->sbc_bmh);
   vol->vh  = vmfs_open_meta_file(vol,VMFS_VH_FILENAME,NULL);

   return(0);
}

/* Dump volume bitmaps */
int vmfs_vol_dump_bitmaps(vmfs_volume_t *vol)
{
   printf("FBB bitmap:\n");
   vmfs_bmh_show(&vol->fbb_bmh);

   printf("\nFDC bitmap:\n");
   vmfs_bmh_show(&vol->fdc_bmh);

   printf("\nPBC bitmap:\n");
   vmfs_bmh_show(&vol->pbc_bmh);

   printf("\nSBC bitmap:\n");
   vmfs_bmh_show(&vol->sbc_bmh);

   return(0);
}

/* Read FDC base information */
static int vmfs_read_fdc_base(vmfs_volume_t *vol)
{
   u_char buf[VMFS_INODE_SIZE];
   vmfs_inode_t inode;
   off_t inode_pos;
   m_u64_t len;

   /* Read the header */
   if (vmfs_vol_read_data(vol,vol->fdc_base,buf,sizeof(buf)) < sizeof(buf))
      return(-1);

   vmfs_bmh_read(&vol->fdc_bmh,buf);

   if (vol->debug_level > 0) {
      printf("FDC bitmap:\n");
      vmfs_bmh_show(&vol->fdc_bmh);
   }

   /* Read the first inode part */
   inode_pos = vol->fdc_base + vmfs_bitmap_get_area_data_addr(&vol->fdc_bmh,0);

   if (fseeko(vol->fd,inode_pos,SEEK_SET) == -1)
      return(-1);
   
   len = vol->fs_info.block_size - (inode_pos - vol->fdc_base);

   if (vol->debug_level > 0) {
      printf("Inodes at @0x%llx\n",(m_u64_t)inode_pos);
      printf("Length: 0x%8.8llx\n",len);
   }

   /* Read the root directory */
   if (fread(buf,vol->fdc_bmh.data_size,1,vol->fd) != 1)
      return(-1);

   vmfs_inode_read(&inode,buf);
   vmfs_read_rootdir(vol,buf);

   /* Read the meta files */
   vmfs_open_all_meta_files(vol);

   /* Dump bitmap info */
   if (vol->debug_level > 0)
      vmfs_vol_dump_bitmaps(vol);

   return(0);
}

/* Create a FS structure */
vmfs_fs_t *vmfs_fs_create(char *filename,int debug_level)
{
   vmfs_fs_t *fs;

   if (!(fs = calloc(1,sizeof(*fs))))
      return NULL;

   if (!(fs->vol = vmfs_vol_create(filename, debug_level)))
      goto err_vol;

   fs->debug_level = debug_level;
   return fs;

 err_vol:
   free(fs);
   return NULL;
}

/* Open a VMFS volume */
int vmfs_fs_open(vmfs_fs_t *fs)
{
   if (vmfs_vol_open(fs->vol))
      return(-1);

   /* Read FS info */
   if (vmfs_fsinfo_read(&fs->fs_info,fs->vol->fd,fs->vol->vmfs_base) == -1) {
      fprintf(stderr,"VMFS: Unable to read FS information\n");
      return(-1);
   }

   if (fs->debug_level > 0)
      vmfs_fsinfo_show(&fs->fs_info);

   /* Compute position of FDC base */
   fs->fdc_base = fs->vol->vmfs_base + VMFS_FDC_BASE;
   /* Temporary */
   fs->vol->fdc_base = fs->fdc_base;
   memcpy(&fs->vol->fs_info, &fs->fs_info, sizeof(fs->fs_info));

   if (fs->debug_level > 0)
      printf("FDC base = @0x%llx\n",(m_u64_t)fs->fdc_base);

   /* Read FDC base information */
   if (vmfs_read_fdc_base(fs->vol) == -1) {
      fprintf(stderr,"VMFS: Unable to read FDC information\n");
      return(-1);
   }
   /* Temporary */
   fs->fbb = fs->vol->fbb;
   fs->fdc = fs->vol->fdc;
   fs->pbc = fs->vol->pbc;
   fs->sbc = fs->vol->sbc;
   fs->vh = fs->vol->vh;
   memcpy(&fs->fbb_bmh, &fs->vol->fbb_bmh, sizeof(fs->fbb_bmh));
   memcpy(&fs->fdc_bmh, &fs->vol->fdc_bmh, sizeof(fs->fdc_bmh));
   memcpy(&fs->pbc_bmh, &fs->vol->pbc_bmh, sizeof(fs->pbc_bmh));
   memcpy(&fs->sbc_bmh, &fs->vol->sbc_bmh, sizeof(fs->sbc_bmh));

   if (fs->debug_level > 0)
      printf("VMFS: filesystem opened successfully\n");
   return(0);
}
