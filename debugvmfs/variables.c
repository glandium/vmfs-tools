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

struct var_member {
   const char *member_name;
   const char *description;
   unsigned short offset;
   unsigned short length;
   char *(*get_value)(char *buf, void *value, short len);
};

struct var_struct {
   int (*dump)(struct var_struct *struct_def, void *value,
               const char *name);
   struct var_member members[];
};

static char *get_value_none(char *buf, void *value, short len);
static char *get_value_uint(char *buf, void *value, short len);
static char *get_value_size(char *buf, void *value, short len);
static char *get_value_string(char *buf, void *value, short len);
static char *get_value_uuid(char *buf, void *value, short len);
static char *get_value_date(char *buf, void *value, short len);
static char *get_value_fs_mode(char *buf, void *value, short len);

#define MEMBER(type, name, desc, format) \
   { # name, desc, offsetof(type, name), \
     sizeof(((type *)0)->name), get_value_ ## format }

struct var_struct vmfs_bitmap = {
   NULL, {
   MEMBER(vmfs_bitmap_header_t, items_per_bitmap_entry,
          "Item per bitmap entry", uint),
   MEMBER(vmfs_bitmap_header_t, bmp_entries_per_area,
          "Bitmap entries per area", uint),
   MEMBER(vmfs_bitmap_header_t, hdr_size, "Header size", size),
   MEMBER(vmfs_bitmap_header_t, data_size, "Data size", size),
   MEMBER(vmfs_bitmap_header_t, area_size, "Area size", size),
   MEMBER(vmfs_bitmap_header_t, area_count, "Area count", uint),
   MEMBER(vmfs_bitmap_header_t, total_items, "Total items", uint),
   { NULL, }
}};

struct var_struct vmfs_fs = {
   NULL, {
   MEMBER(vmfs_fsinfo_t, vol_version, "Volume Version", uint),
   MEMBER(vmfs_fsinfo_t, version, "Version", uint),
   MEMBER(vmfs_fsinfo_t, label, "Label", string),
   MEMBER(vmfs_fsinfo_t, mode, "Mode", fs_mode),
   MEMBER(vmfs_fsinfo_t, uuid, "UUID", uuid),
   MEMBER(vmfs_fsinfo_t, ctime, "Creation time", date),
   MEMBER(vmfs_fsinfo_t, block_size, "Block size", size),
   MEMBER(vmfs_fsinfo_t, subblock_size, "Subblock size", size),
   MEMBER(vmfs_fsinfo_t, fdc_header_size, "FDC Header size", size),
   MEMBER(vmfs_fsinfo_t, fdc_bitmap_count, "FDC Bitmap count", uint),
   { NULL, }
}};

struct var_struct vmfs_lvm = {
   NULL, {
   MEMBER(vmfs_lvminfo_t, uuid, "UUID", uuid),
   MEMBER(vmfs_lvminfo_t, size, "Size", size),
   MEMBER(vmfs_lvminfo_t, blocks, "Blocks", uint),
   MEMBER(vmfs_lvminfo_t, num_extents, "Num. Extents", uint),
   { NULL, }
}};

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

static const char * const units[] = {
   "",
   " KiB",
   " MiB",
   " GiB",
   " TiB",
};

static char *human_readable_size(char *buf, uint64_t size)
{
   int scale = 0;
   for (scale = 0; (size >> scale) >= 1024; scale += 10);

   if (size & ((1L << scale) - 1))
      sprintf(buf, "%.2f%s", (float) size / (1L << scale), units[scale / 10]);
   else
      sprintf(buf, "%"PRIu64"%s", size >> scale, units[scale / 10]);

   return buf;
}

static char *var_value(char *buf, char *struct_buf, struct var_member *member)
{
   return member->get_value(buf, &struct_buf[member->offset], member->length);
}

static char *get_value_uint(char *buf, void *value, short len)
{
   switch (len) {
   case 4:
      sprintf(buf, "%" PRIu32, *((uint32_t *)value));
      return buf;
   case 8:
      sprintf(buf, "%" PRIu64, *((uint64_t *)value));
      return buf;
   }
   return get_value_none(buf, value, len);
}

static char *get_value_size(char *buf, void *value, short len)
{
   switch (len) {
   case 4:
      return human_readable_size(buf, *((uint32_t *)value));
   case 8:
      return human_readable_size(buf, *((uint64_t *)value));
   }
   return get_value_none(buf, value, len);
}

static char *get_value_string(char *buf, void *value, short len)
{
   strcpy(buf, *((char **)value));
   return buf;
}

static char *get_value_uuid(char *buf, void *value, short len)
{
   return m_uuid_to_str((u_char *)value,buf);
}

static char *get_value_date(char *buf, void *value, short len)
{
   return m_ctime((time_t *)(uint32_t *)value, buf, 256);
}

static char *get_value_fs_mode(char *buf, void *value, short len)
{
   sprintf(buf, "%s", vmfs_fs_mode_to_str(*((uint32_t *)value)));
   return buf;
}

static char *get_value_none(char *buf, void *value, short len)
{
   strcpy(buf, "Don't know how to display");
   return buf;
}

static int longest_member_desc(struct var_struct *struct_def)
{
   struct var_member *m;
   int len = 0, curlen;
   for (m = struct_def->members; m->member_name; m++) {
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
   struct var_struct *struct_def = NULL;
   struct var_member *member;
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
            struct_def = &vmfs_bitmap;
         } else if (!strcmp(current, "fdc")) {
            struct_buf = (char *)&fs->fdc->bmh;
            struct_def = &vmfs_bitmap;
         } else if (!strcmp(current, "pbc")) {
            struct_buf = (char *)&fs->pbc->bmh;
            struct_def = &vmfs_bitmap;
         } else if (!strcmp(current, "sbc")) {
            struct_buf = (char *)&fs->sbc->bmh;
            struct_def = &vmfs_bitmap;
         } else if (!strcmp(current, "fs")) {
            struct_buf = (char *)&fs->fs_info;
            struct_def = &vmfs_fs;
         } else if (!strcmp(current, "lvm")) {
            struct_buf = (char *)&fs->lvm->lvm_info;
            struct_def = &vmfs_lvm;
         } else
            return(1);
         if (!next) {
            struct var_member *member;
            char format[16];
            sprintf(format, "%%%ds: %%s\n", longest_member_desc(struct_def));
            for (member = struct_def->members; member->member_name; member++)
               printf(format, member->description,
                              var_value(buf, struct_buf, member));
         }
      } else {
         member = struct_def->members;
         while(member->member_name && strcmp(member->member_name, current))
            member++;
         if (member->member_name)
            printf("%s: %s\n", member->description,
                               var_value(buf, struct_buf, member));
      }
   }
   return(0);
}
