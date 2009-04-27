/* 
 * VMFS volumes.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "vmfs.h"
#include "scsi.h"

/* Read a data block from the physical volume */
static ssize_t vmfs_vol_read_data(const vmfs_volume_t *vol,off_t pos,
                                  u_char *buf,size_t len)
{
   if (lseek(vol->fd,pos,SEEK_SET) == -1)
      return(-1);

   return(read(vol->fd,buf,len));
}

/* Read a raw block of data on logical volume */
ssize_t vmfs_vol_read(const vmfs_volume_t *vol,off_t pos,u_char *buf,size_t len)
{
   pos += vol->vmfs_base + 0x1000000;

   return(vmfs_vol_read_data(vol,pos,buf,len));
}

/* Write a data block to the physical volume */
static ssize_t vmfs_vol_write_data(const vmfs_volume_t *vol,off_t pos,
                                   const u_char *buf,size_t len)
{
   if (lseek(vol->fd,pos,SEEK_SET) == -1)
      return(-1);

   return(write(vol->fd,buf,len));
}

/* Write a raw block of data on logical volume */
ssize_t vmfs_vol_write(const vmfs_volume_t *vol,off_t pos,const u_char *buf,
                       size_t len)
{
   pos += vol->vmfs_base + 0x1000000;

   return(vmfs_vol_write_data(vol,pos,buf,len));
}

/* Volume reservation */
int vmfs_vol_reserve(const vmfs_volume_t *vol)
{
   if (!vol->scsi_reservation)
      return(-1);

   return(scsi_reserve(vol->fd));
}

/* Volume release */
int vmfs_vol_release(const vmfs_volume_t *vol)
{
   if (!vol->scsi_reservation)
      return(-1);

   return(scsi_release(vol->fd));
}

/* 
 * Check if physical volume support reservation.
 * TODO: We should probably check some capabilities info.
 */
int vmfs_vol_check_reservation(vmfs_volume_t *vol)
{
   int res[2];

   /* The device must be a block device */
   if (!vol->is_blkdev)
      return(0);

   /* Try SCSI commands */
   res[0] = scsi_reserve(vol->fd);
   res[1] = scsi_release(vol->fd);

   /* Error with the commands */
   if ((res[0] < 0) || (res[1] < 0))
      return(0);

   vol->scsi_reservation = 1;
   return(1);
}

/* Read volume information */
static int vmfs_volinfo_read(vmfs_volinfo_t *vol,int fd)
{
   u_char buf[1024];

   if (lseek(fd,VMFS_VOLINFO_BASE,SEEK_SET) == -1)
      return(-1);

   if (read(fd,buf,sizeof(buf)) != sizeof(buf))
      return(-1);

   vol->magic = read_le32(buf,VMFS_VOLINFO_OFS_MAGIC);

   if (vol->magic != VMFS_VOLINFO_MAGIC) {
      fprintf(stderr,"VMFS VolInfo: invalid magic number 0x%8.8x\n",
              vol->magic);
      return(-1);
   }

   vol->version = read_le32(buf,VMFS_VOLINFO_OFS_VER);
   vol->lun = buf[VMFS_VOLINFO_OFS_LUN];

   vol->name = strndup((char *)buf+VMFS_VOLINFO_OFS_NAME,
                       VMFS_VOLINFO_OFS_NAME_SIZE);

   read_uuid(buf,VMFS_VOLINFO_OFS_UUID,&vol->uuid);

   vol->size    = read_le64(buf,VMFS_LVMINFO_OFS_SIZE);
   vol->blocks  = read_le64(buf,VMFS_LVMINFO_OFS_BLKS);
   vol->num_segments = read_le32(buf,VMFS_LVMINFO_OFS_NUM_SEGMENTS);
   vol->first_segment = read_le32(buf,VMFS_LVMINFO_OFS_FIRST_SEGMENT);
   vol->last_segment = read_le32(buf,VMFS_LVMINFO_OFS_LAST_SEGMENT);
   vol->num_extents = read_le32(buf,VMFS_LVMINFO_OFS_NUM_EXTENTS);

   read_uuid(buf,VMFS_LVMINFO_OFS_UUID,&vol->lvm_uuid);
#ifdef VMFS_CHECK
   {
      /* The LVM UUID also appears as a string, so we can check whether our
         formatting function is correct. */
      char uuidstr1[M_UUID_BUFLEN], uuidstr2[M_UUID_BUFLEN];
      memcpy(uuidstr1,buf+VMFS_LVMINFO_OFS_UUID_STR,M_UUID_BUFLEN-1);
      uuidstr1[M_UUID_BUFLEN-1] = 0;
      if (memcmp(m_uuid_to_str(vol->lvm_uuid,uuidstr2),uuidstr1,M_UUID_BUFLEN-1)) {
         fprintf(stderr, "uuid mismatch:\n%s\n%s\n",uuidstr1,uuidstr2);
         return(-1);
      }
   }
#endif

   return(0);
}

/* Show volume information */
void vmfs_vol_show(const vmfs_volume_t *vol)
{
   char uuid_str[M_UUID_BUFLEN];

   printf("Physical Volume Information:\n");
   printf("  - UUID    : %s\n",m_uuid_to_str(vol->vol_info.uuid,uuid_str));
   printf("  - LUN     : %d\n",vol->vol_info.lun);
   printf("  - Version : %d\n",vol->vol_info.version);
   printf("  - Name    : %s\n",vol->vol_info.name);
   printf("  - Num. Segments : %u\n",vol->vol_info.num_segments);
   printf("  - First Segment : %u\n",vol->vol_info.first_segment);
   printf("  - Last Segment  : %u\n",vol->vol_info.last_segment);

   printf("\n");
}

/* Create a volume structure */
vmfs_volume_t *vmfs_vol_create(const char *filename,vmfs_flags_t flags)
{
   vmfs_volume_t *vol;
   struct stat st;

   if (!(vol = calloc(1,sizeof(*vol))))
      return NULL;

   if (!(vol->filename = strdup(filename)))
      goto err_filename;

   if ((vol->fd = open(vol->filename,O_RDWR)) < 0) {
      perror("open");
      goto err_open;
   }

   vol->flags = flags;
   fstat(vol->fd,&st);
   vol->is_blkdev=S_ISBLK(st.st_mode);
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

   if (vol->is_blkdev && (scsi_get_lun(vol->fd) != vol->vol_info.lun))
      fprintf(stderr,"VMFS: Warning: Lun ID mismatch on %s\n", vol->filename);

   if (vol->flags.debug_level > 0) {
      vmfs_vol_show(vol);
      printf("VMFS: volume opened successfully\n");
   }
   return(0);
}
