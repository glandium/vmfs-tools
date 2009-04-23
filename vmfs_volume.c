/* 
 * VMFS volumes.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "vmfs.h"

#define VMFS_FDC_BASE       0x1400000

/* Read a data block from the physical volume */
ssize_t vmfs_vol_read_data(vmfs_volume_t *vol,off_t pos,u_char *buf,size_t len)
{
   if (fseeko(vol->fd,pos,SEEK_SET) != 0)
      return(-1);

   return(fread(buf,1,len,vol->fd));
}

/* Read a block */
ssize_t vmfs_vol_read(vmfs_volume_t *vol,m_u32_t blk,off_t offset,
                      u_char *buf,size_t len)
{
   off_t pos;

   pos  = (m_u64_t)blk * vmfs_vol_get_blocksize(vol);
   pos += vol->vmfs_base + 0x1000000;
   pos += offset;

   return(vmfs_vol_read_data(vol,pos,buf,len));
}

/* Read volume information */
int vmfs_volinfo_read(vmfs_volinfo_t *vol,FILE *fd)
{
   u_char buf[1024];

   if (fseeko(fd,VMFS_VOLINFO_BASE,SEEK_SET) != 0)
      return(-1);

   if (fread(buf,sizeof(buf),1,fd) != 1)
      return(-1);

   vol->magic   = read_le32(buf,VMFS_VOLINFO_OFS_MAGIC);

   if (vol->magic != VMFS_VOLINFO_MAGIC) {
      fprintf(stderr,"VMFS VolInfo: invalid magic number 0x%8.8x\n",vol->magic);
      return(-1);
   }

   vol->version = read_le32(buf,VMFS_VOLINFO_OFS_VER);

   vol->name = strndup((char *)buf+VMFS_VOLINFO_OFS_NAME,
                       VMFS_VOLINFO_OFS_NAME_SIZE);

   memcpy(vol->uuid,buf+VMFS_VOLINFO_OFS_UUID,sizeof(vol->uuid));

   vol->size    = read_le64(buf,VMFS_LVMINFO_OFS_SIZE);
   vol->blocks  = read_le64(buf,VMFS_LVMINFO_OFS_BLKS);
   vol->num_segments = read_le32(buf,VMFS_LVMINFO_OFS_NUM_SEGMENTS);
   vol->first_segment = read_le32(buf,VMFS_LVMINFO_OFS_FIRST_SEGMENT);
   vol->last_segment = read_le32(buf,VMFS_LVMINFO_OFS_LAST_SEGMENT);
   vol->num_extents = read_le32(buf,VMFS_LVMINFO_OFS_NUM_EXTENTS);

   memcpy(vol->lvm_uuid,buf+VMFS_LVMINFO_OFS_UUID,sizeof(vol->lvm_uuid));

   return(0);
}

/* Show volume information */
void vmfs_volinfo_show(vmfs_volinfo_t *vol)
{
   char uuid_str[M_UUID_BUFLEN];

   printf("VMFS Volume Information:\n");
   printf("  - Version : %d\n",vol->version);
   printf("  - Name    : %s\n",vol->name);
   printf("  - UUID    : %s\n",m_uuid_to_str(vol->uuid,uuid_str));
   printf("\nLVM Information:\n");
   printf("  - Size    : %llu Gb\n",vol->size / (1024*1048576));
   printf("  - Blocks  : %llu\n",vol->blocks);
   printf("  - UUID    : %s\n",m_uuid_to_str(vol->lvm_uuid,uuid_str));
   printf("  - Num. Segments : %u\n",vol->num_segments);
   printf("  - First Segment : %u\n",vol->first_segment);
   printf("  - Last Segment  : %u\n",vol->last_segment);
   printf("  - Num. Extents  : %u\n",vol->num_extents);


   printf("\n");
}

/* Create a volume structure */
vmfs_volume_t *vmfs_vol_create(char *filename,int debug_level)
{
   vmfs_volume_t *vol;

   if (!(vol = calloc(1,sizeof(*vol))))
      return NULL;

   if (!(vol->filename = strdup(filename)))
      goto err_filename;

   if (!(vol->fd = fopen(vol->filename,"r"))) {
      perror("fopen");
      goto err_open;
   }

   vol->debug_level = debug_level;
   return vol;

 err_open:
   free(vol->filename);
 err_filename:
   free(vol);
   return NULL;
}

/* Open a VMFS volume */
int vmfs_vol_open(vmfs_volume_t *vol)
{
   vol->vmfs_base = VMFS_VOLINFO_BASE;

   /* Read volume information */
   if (vmfs_volinfo_read(&vol->vol_info,vol->fd) == -1) {
      fprintf(stderr,"VMFS: Unable to read volume information\n");
      return(-1);
   }

   if (vol->debug_level > 0)
      vmfs_volinfo_show(&vol->vol_info);

   /* Stop here for extents that are not the first */
   if (vol->vol_info.first_segment != 0)
      return(0);

   /* Read FS info */
   if (vmfs_fsinfo_read(&vol->fs_info,vol->fd,vol->vmfs_base) == -1) {
      fprintf(stderr,"VMFS: Unable to read FS information\n");
      return(-1);
   }

   if (vol->debug_level > 0)
      vmfs_fsinfo_show(&vol->fs_info);

   /* Compute position of FDC base */
   vol->fdc_base = vol->vmfs_base + VMFS_FDC_BASE;

   if (vol->debug_level > 0)
      printf("FDC base = @0x%llx\n",(m_u64_t)vol->fdc_base);

   /* Read FDC base information */
   if (vmfs_read_fdc_base(vol) == -1) {
      fprintf(stderr,"VMFS: Unable to read FDC information\n");
      return(-1);
   }

   if (vol->debug_level > 0)
      printf("VMFS: volume opened successfully\n");
   return(0);
}
