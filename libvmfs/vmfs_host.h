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
#ifndef VMFS_HOST_H
#define VMFS_HOST_H

/* Initialize host info (UUID,uptime,...) */
int vmfs_host_init(void);

/* Show host info */
void vmfs_host_show_info(void);

/* Get host uptime (in usecs) */
uint64_t vmfs_host_get_uptime(void);

/* Get host UUID */
void vmfs_host_get_uuid(uuid_t dst);

#endif
