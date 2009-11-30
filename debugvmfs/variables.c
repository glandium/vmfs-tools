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

enum var_format {
   NONE,
   UINT,
};

struct var_struct {
   const char *member_name;
   const char *description;
   unsigned short offset;
   unsigned short length;
   enum var_format format;
};

#define MEMBER(type, name, desc, format) \
   { # name, desc, offsetof(type, name), \
     sizeof(((type *)0)->name), format }

struct var_struct vmfs_bitmap[] = {
   MEMBER(vmfs_bitmap_header_t, items_per_bitmap_entry,
          "Item per bitmap entry", UINT),
   MEMBER(vmfs_bitmap_header_t, bmp_entries_per_area,
          "Bitmap entries per area", UINT),
   MEMBER(vmfs_bitmap_header_t, hdr_size, "Header size", UINT),
   MEMBER(vmfs_bitmap_header_t, data_size, "Data size", UINT),
   MEMBER(vmfs_bitmap_header_t, area_size, "Area size", UINT),
   MEMBER(vmfs_bitmap_header_t, area_count, "Area count", UINT),
   MEMBER(vmfs_bitmap_header_t, total_items, "Total items", UINT),
   { NULL, }
};

static char *var_value(char *buf, char *struct_buf, struct var_struct *member)
{
   switch(member->format) {
   case UINT:
      switch (member->length) {
      case 4:
         sprintf(buf, "%" PRIu32, *((uint32_t *)&struct_buf[member->offset]));
         return buf;
      case 8:
         sprintf(buf, "%" PRIu64, *((uint64_t *)&struct_buf[member->offset]));
         return buf;
      default:
         goto unknown;
      }
   default:
      goto unknown;
   }
unknown:
   strcpy(buf, "Don't know how to display");
   return buf;
}

int vmfs_show_variable(const vmfs_fs_t *fs, const char *name)
{
   char split_name[256];
   char buf[256];
   char *current, *next;
   struct var_struct *member = NULL;
   char *struct_buf = NULL;

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
            struct_buf = (char *)&fs->fbb->bmh;
         else if (!strcmp(current, "fdc"))
            struct_buf = (char *)&fs->fdc->bmh;
         else if (!strcmp(current, "pbc"))
            struct_buf = (char *)&fs->pbc->bmh;
         else if (!strcmp(current, "sbc"))
            struct_buf = (char *)&fs->sbc->bmh;
         else
            return(1);
         member = vmfs_bitmap;
         if (!next)
            for(; member->member_name; member++)
               printf("%s: %s\n", member->description,
                                  var_value(buf, struct_buf, member));
      } else {
         while(member->member_name && strcmp(member->member_name, current))
            member++;
         if (member->member_name)
            printf("%s: %s\n", member->description,
                               var_value(buf, struct_buf, member));
      }
   }
   return(0);
}
