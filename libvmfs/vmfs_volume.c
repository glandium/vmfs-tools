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
 * VMFS volumes.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>

#include "vmfs.h"
#include "scsi.h"

/* Read a raw block of data on logical volume */
static ssize_t vmfs_vol_read(const vmfs_device_t *dev,off_t pos,
                             u_char *buf,size_t len)
{
   vmfs_volume_t *vol = (vmfs_volume_t *) dev;
   pos += vol->vmfs_base + 0x1000000;

   return(m_pread(vol->fd,buf,len,pos));
}

/* Write a raw block of data on logical volume */
static ssize_t vmfs_vol_write(const vmfs_device_t *dev,off_t pos,
                              const u_char *buf,size_t len)
{
   vmfs_volume_t *vol = (vmfs_volume_t *) dev;
   pos += vol->vmfs_base + 0x1000000;

   return(m_pwrite(vol->fd,buf,len,pos));
}

/* Volume reservation */
static int vmfs_vol_reserve(const vmfs_device_t *dev, off_t pos)
{
   vmfs_volume_t *vol = (vmfs_volume_t *) dev;
   return(scsi_reserve(vol->fd));
}

/* Volume release */
static int vmfs_vol_release(const vmfs_device_t *dev, off_t pos)
{
   vmfs_volume_t *vol = (vmfs_volume_t *) dev;
   return(scsi_release(vol->fd));
}

/* 
 * Check if physical volume support reservation.
 * TODO: We should probably check some capabilities info.
 */
static int vmfs_vol_check_reservation(vmfs_volume_t *vol)
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

   vol->dev.reserve = vmfs_vol_reserve;
   vol->dev.release = vmfs_vol_release;
   return(1);
}

/* Read volume information */
static int vmfs_volinfo_read(vmfs_volume_t *volume)
{
   DECL_ALIGNED_BUFFER(buf,1024);
   vmfs_volinfo_t *vol = &volume->vol_info;

   if (m_pread(volume->fd,buf,buf_len,volume->vmfs_base) != buf_len)
      return(-1);

   vol->magic = read_le32(buf,VMFS_VOLINFO_OFS_MAGIC);

   if (vol->magic != VMFS_VOLINFO_MAGIC) {
      fprintf(stderr,"VMFS VolInfo: invalid magic number 0x%8.8x\n",
              vol->magic);
      return(-1);
   }

   vol->version = read_le32(buf,VMFS_VOLINFO_OFS_VER);
   vol->size = read_le32(buf,VMFS_VOLINFO_OFS_SIZE);
   vol->lun = buf[VMFS_VOLINFO_OFS_LUN];

   vol->name = strndup((char *)buf+VMFS_VOLINFO_OFS_NAME,
                       VMFS_VOLINFO_OFS_NAME_SIZE);

   read_uuid(buf,VMFS_VOLINFO_OFS_UUID,&vol->uuid);

   vol->lvm_size = read_le64(buf,VMFS_LVMINFO_OFS_SIZE);
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

      if (memcmp(m_uuid_to_str(vol->lvm_uuid,uuidstr2),uuidstr1,
                 M_UUID_BUFLEN-1)) 
      {
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
   printf("  - Size    : %.2f GB\n",(float)vol->vol_info.size / (4*1048576));
   printf("  - Num. Segments : %u\n",vol->vol_info.num_segments);
   printf("  - First Segment : %u\n",vol->vol_info.first_segment);
   printf("  - Last Segment  : %u\n",vol->vol_info.last_segment);

   printf("\n");
}

/* Open a VMFS volume */
vmfs_volume_t *vmfs_vol_open(const char *filename,vmfs_flags_t flags)
{
   vmfs_volume_t *vol;
   struct stat st;
   int file_flags;

   if (!(vol = calloc(1,sizeof(*vol))))
      return NULL;

   if (!(vol->filename = strdup(filename)))
      goto err_filename;

   file_flags = (flags.read_write) ? O_RDWR : O_RDONLY;

   if ((vol->fd = open(vol->filename,file_flags)) < 0) {
      perror("open");
      goto err_open;
   }

   vol->flags = flags;
   fstat(vol->fd,&st);
   vol->is_blkdev = S_ISBLK(st.st_mode);
#if defined(O_DIRECT) || defined(DIRECTIO_ON)
   if (vol->is_blkdev)
#ifdef O_DIRECT
      fcntl(vol->fd, F_SETFL, O_DIRECT);
#else
#ifdef DIRECTIO_ON
      directio(vol->fd, DIRECTIO_ON);
#endif
#endif
#endif

   vol->vmfs_base = VMFS_VOLINFO_BASE;

   /* Read volume information */
   if (vmfs_volinfo_read(vol) == -1) {
      DECL_ALIGNED_BUFFER(buf,512);
      uint16_t magic;
      fprintf(stderr,"VMFS: Unable to read volume information\n");
      fprintf(stderr,"Trying to find partitions\n");
      m_pread(vol->fd,buf,buf_len,0);
      magic = read_le16(buf, 510);
      if ((magic == 0xaa55) && (buf[450] == 0xfb)) {
         vol->vmfs_base += read_le32(buf, 454) * 512;
         if (vmfs_volinfo_read(vol) == -1)
            goto err_open;
      } else
         goto err_open;
   }

   /* We support only VMFS3 */
   if (vol->vol_info.version != 3) {
      fprintf(stderr,"VMFS: Unsupported version %u\n",vol->vol_info.version);
      goto err_open;
   }

   if (vol->is_blkdev && (scsi_get_lun(vol->fd) != vol->vol_info.lun))
      fprintf(stderr,"VMFS: Warning: Lun ID mismatch on %s\n", vol->filename);

   vmfs_vol_check_reservation(vol);

   if (vol->flags.debug_level > 0) {
      vmfs_vol_show(vol);
      printf("VMFS: volume opened successfully\n");
   }

   vol->dev.read = vmfs_vol_read;
   vol->dev.write = vmfs_vol_write;

   return vol;

 err_open:
   free(vol->filename);
 err_filename:
   free(vol);
   return NULL;
}

/* Close a VMFS volume */
void vmfs_vol_close(vmfs_volume_t *vol)
{
   if (!vol)
      return;
   close(vol->fd);
   free(vol->filename);
   free(vol->vol_info.name);
   free(vol);
}
