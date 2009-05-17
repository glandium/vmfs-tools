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
 * VMFS host.
 */

#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include "vmfs.h"

static uuid_t host_uuid;
static struct timeval host_tv_start;

/* Initialize host info (UUID,uptime,...) */
int vmfs_host_init(void)
{
   uuid_generate_time(host_uuid);
   gettimeofday(&host_tv_start,NULL);
   return(0);
}

/* Show host info */
void vmfs_host_show_info(void)
{   
   char uuid_str[M_UUID_BUFLEN];

   printf("Host UUID   : %s\n",m_uuid_to_str(host_uuid,uuid_str));
   printf("Host Uptime : %"PRIu64" usecs\n",vmfs_host_get_uptime());
}

/* Get host uptime (in usecs) */
uint64_t vmfs_host_get_uptime(void)
{
   struct timeval cur_time,delta;
   uint64_t uptime;

   gettimeofday(&cur_time,NULL);
   timersub(&cur_time,&host_tv_start,&delta);
   
   uptime = ((uint64_t)delta.tv_sec * 1000000) + delta.tv_usec;
   return(uptime);
}

/* Get host UUID */
void vmfs_host_get_uuid(uuid_t dst)
{
   uuid_copy(dst,host_uuid);
}
