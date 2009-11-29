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

#include <string.h>
#include "vmfs.h"

struct var_struct {
   const char *member_name;
   const char *description;
   unsigned short offset;
};

#define MEMBER(type, name, desc) { # name, desc, offsetof(type, name) }

struct var_struct vmfs_bitmap[] = {
   MEMBER(vmfs_bitmap_header_t, items_per_bitmap_entry,
          "Item per bitmap entry"),
   MEMBER(vmfs_bitmap_header_t, bmp_entries_per_area,
          "Bitmap entries per area"),
   MEMBER(vmfs_bitmap_header_t, hdr_size, "Header size"),
   MEMBER(vmfs_bitmap_header_t, data_size, "Data size"),
   MEMBER(vmfs_bitmap_header_t, area_size, "Area size"),
   MEMBER(vmfs_bitmap_header_t, area_count, "Area count"),
   MEMBER(vmfs_bitmap_header_t, total_items, "Total items"),
   { NULL, }
};

int vmfs_show_variable(const vmfs_fs_t *fs, const char *name)
{
   char split_name[256];
   char *current, *next;
   struct var_struct *member = NULL;
   char *buf = NULL;

   strncpy(split_name, name, 255);
   split_name[255] = 0;

   for (next = split_name; next;) {
      current = next;
      next = index(next, '.');
      if (next)
         *(next++) = 0;
      if (*current == 0)
         return(1);
      if (current == split_name) {
         if (!strcmp(current, "fbb"))
            buf = (char *)&fs->fbb->bmh;
         else if (!strcmp(current, "fdc"))
            buf = (char *)&fs->fdc->bmh;
         else if (!strcmp(current, "pbc"))
            buf = (char *)&fs->pbc->bmh;
         else if (!strcmp(current, "sbc"))
            buf = (char *)&fs->sbc->bmh;
         else
            return(1);
         member = vmfs_bitmap;
         if (!next)
            for(; member->member_name; member++)
               printf("%s: %d\n", member->description,
                                  *((uint32_t *)&buf[member->offset]));
      } else {
         while(member->member_name && strcmp(member->member_name, current))
            member++;
         if (member->member_name)
            printf("%s: %d\n", member->description,
                               *((uint32_t *)&buf[member->offset]));
      }
   }
   return(0);
}
