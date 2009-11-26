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
#ifndef SCSI_H
#define SCSI_H

#ifdef __linux__
#include <scsi/scsi.h>
#include <sys/ioctl.h>

struct scsi_idlun {
   int four_in_one;
   int host_unique_id;
};
#endif

static inline int scsi_get_lun(int fd) 
{
#if __linux__
   struct scsi_idlun idlun;

   if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun))
#endif
      return(-1);

#if __linux__
   return((idlun.four_in_one >> 8) & 0xff);
#endif
}

/* Send a SCSI "reserve" command */
int scsi_reserve(int fd);

/* Send a SCSI "release" command */
int scsi_release(int fd);

#endif
