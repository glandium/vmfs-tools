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
   STRING,
   UUID,
   DATE,
   FS_MODE,
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

struct var_struct vmfs_fs[] = {
   MEMBER(vmfs_fsinfo_t, vol_version, "Volume Version", UINT),
   MEMBER(vmfs_fsinfo_t, version, "Version", UINT),
   MEMBER(vmfs_fsinfo_t, label, "Label", STRING),
   MEMBER(vmfs_fsinfo_t, mode, "Mode", FS_MODE),
   MEMBER(vmfs_fsinfo_t, uuid, "UUID", UUID),
   MEMBER(vmfs_fsinfo_t, ctime, "Creation time", DATE),
   MEMBER(vmfs_fsinfo_t, block_size, "Block size", UINT),
   MEMBER(vmfs_fsinfo_t, subblock_size, "Subblock size", UINT),
   MEMBER(vmfs_fsinfo_t, fdc_header_size, "FDC Header size", UINT),
   MEMBER(vmfs_fsinfo_t, fdc_bitmap_count, "FDC Bitmap count", UINT),
   { NULL, }
};

struct var_struct vmfs_lvm[] = {
   MEMBER(vmfs_lvminfo_t, uuid, "UUID", UUID),
   MEMBER(vmfs_lvminfo_t, size, "Size", UINT),
   MEMBER(vmfs_lvminfo_t, blocks, "Blocks", UINT),
   MEMBER(vmfs_lvminfo_t, num_extents, "Num. Extents", UINT),
   { NULL, }
};

/* Get string corresponding to specified mode */
static char *vmfs_fs_mode_to_str(uint32_t mode)
{
   /* only two lower bits seem to be significant */
   switch(mode & 0x03) {
      case 0x00:
         return "private";
      case 0x01:
      case 0x03:
         return "shared";
      case 0x02:
         return "public";
   }

   /* should not happen */
   return NULL;
}

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
   case STRING:
      strcpy(buf, *((char **)&struct_buf[member->offset]));
      return buf;
   case UUID:
      return m_uuid_to_str((u_char *)&struct_buf[member->offset],buf);
   case DATE:
      return m_ctime((time_t *)(uint32_t *)&struct_buf[member->offset],
                     buf, 256);
   case FS_MODE:
      sprintf(buf, "%s",
              vmfs_fs_mode_to_str(*((uint32_t *)&struct_buf[member->offset])));
      return buf;
   default:
      goto unknown;
   }
unknown:
   strcpy(buf, "Don't know how to display");
   return buf;
}

static int longest_member_desc(struct var_struct *member)
{
   struct var_struct *m;
   int len = 0, curlen;
   for (m = member; m->member_name; m++) {
      curlen = strlen(m->description);
      if (curlen > len)
         len = curlen;
   }
   return len;
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
         if (!strcmp(current, "fbb")) {
            struct_buf = (char *)&fs->fbb->bmh;
            member = vmfs_bitmap;
         } else if (!strcmp(current, "fdc")) {
            struct_buf = (char *)&fs->fdc->bmh;
            member = vmfs_bitmap;
         } else if (!strcmp(current, "pbc")) {
            struct_buf = (char *)&fs->pbc->bmh;
            member = vmfs_bitmap;
         } else if (!strcmp(current, "sbc")) {
            struct_buf = (char *)&fs->sbc->bmh;
            member = vmfs_bitmap;
         } else if (!strcmp(current, "fs")) {
            struct_buf = (char *)&fs->fs_info;
            member = vmfs_fs;
         } else if (!strcmp(current, "lvm")) {
            struct_buf = (char *)&fs->lvm->lvm_info;
            member = vmfs_lvm;
         } else
            return(1);
         if (!next) {
            char format[16];
            sprintf(format, "%%%ds: %%s\n", longest_member_desc(member));
            for (; member->member_name; member++)
               printf(format, member->description,
                              var_value(buf, struct_buf, member));
         }
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
