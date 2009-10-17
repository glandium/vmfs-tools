/*
 * vmfs-tools - Tools to access VMFS filesystems
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
#ifndef VMFS_DEVICE_H
#define VMFS_DEVICE_H

#include "vmfs.h"

struct vmfs_device {
   ssize_t (*read)(const vmfs_device_t *dev, off_t pos,
                   u_char *buf, size_t len);
   ssize_t (*write)(const vmfs_device_t *dev, off_t pos,
                    const u_char *buf, size_t len);
   int (*reserve)(const vmfs_device_t *dev, off_t pos);
   int (*release)(const vmfs_device_t *dev, off_t pos);
};

static inline ssize_t vmfs_device_read(const vmfs_device_t *dev, off_t pos,
                                       u_char *buf, size_t len)
{
   return dev->read(dev, pos, buf, len);
}

static inline ssize_t vmfs_device_write(const vmfs_device_t *dev, off_t pos,
                                        const u_char *buf, size_t len)
{
   return dev->write(dev, pos, buf, len);
}

static inline int vmfs_device_reserve(const vmfs_device_t *dev, off_t pos)
{
   if (dev->reserve)
     return dev->reserve(dev, pos);
   return 0;
}

static inline int vmfs_device_release(const vmfs_device_t *dev, off_t pos)
{
   if (dev->release)
     return dev->release(dev, pos);
   return 0;
}

#endif
