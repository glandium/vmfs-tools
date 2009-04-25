#ifndef SCSI_H
#define SCSI_H

#include <scsi/scsi.h>
#include <sys/ioctl.h>

struct scsi_idlun {
   int four_in_one;
   int host_unique_id;
};

static inline int scsi_get_lun(int fd) 
{
   struct scsi_idlun idlun;

   if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun))
      return(-1);

   return((idlun.four_in_one >> 8) & 0xff);
}

/* Send a SCSI "reserve" command */
int scsi_reserve(int fd);

/* Send a SCSI "release" command */
int scsi_release(int fd);

#endif
