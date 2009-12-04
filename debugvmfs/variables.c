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

struct var_struct;

struct var_member {
   const char *member_name;
   union {
      const char *description;
      struct var_struct *subvar;
   };
   unsigned short offset;
   unsigned short length;
   char *(*get_value)(char *buf, void *value, short len);
};

struct var_struct {
   int (*dump)(struct var_struct *struct_def, void *value,
               const char *name);
   struct var_member members[];
};

static int struct_dump(struct var_struct *struct_def, void *value,
                       const char *name);

static char *get_value_none(char *buf, void *value, short len);
static char *get_value_uint(char *buf, void *value, short len);
static char *get_value_size(char *buf, void *value, short len);
static char *get_value_string(char *buf, void *value, short len);
static char *get_value_uuid(char *buf, void *value, short len);
static char *get_value_date(char *buf, void *value, short len);
static char *get_value_fs_mode(char *buf, void *value, short len);
static char *get_value_bitmap_used(char *buf, void *value, short len);
static char *get_value_bitmap_free(char *buf, void *value, short len);

#define MEMBER(type, name, desc, format) \
   { # name, { desc }, offsetof(type, name), \
     sizeof(((type *)0)->name), get_value_ ## format }

#define MEMBER2(type, sub, name, desc, format) \
   { # name, { desc }, offsetof(type, sub.name), \
     sizeof(((type *)0)->sub.name), get_value_ ## format }

#define SUBVAR(type, name, struct_def) \
   { # name, { subvar: &struct_def }, offsetof(type, name), \
     sizeof(((type *)0)->name), NULL }

#define VIRTUAL_MEMBER(name, desc, format) \
   MEMBER(struct { char name[0]; }, name, desc, format)

struct var_struct vmfs_bitmap = {
   struct_dump, {
   MEMBER2(vmfs_bitmap_t, bmh, items_per_bitmap_entry,
           "Item per bitmap entry", uint),
   MEMBER2(vmfs_bitmap_t, bmh, bmp_entries_per_area,
           "Bitmap entries per area", uint),
   MEMBER2(vmfs_bitmap_t, bmh, hdr_size, "Header size", size),
   MEMBER2(vmfs_bitmap_t, bmh, data_size, "Data size", size),
   MEMBER2(vmfs_bitmap_t, bmh, area_size, "Area size", size),
   MEMBER2(vmfs_bitmap_t, bmh, area_count, "Area count", uint),
   MEMBER2(vmfs_bitmap_t, bmh, total_items, "Total items", uint),
   VIRTUAL_MEMBER(used_items, "Used items", bitmap_used),
   VIRTUAL_MEMBER(free_items, "Free items", bitmap_free),
   { NULL, }
}};

struct var_struct vmfs_fs = {
   struct_dump, {
   MEMBER2(vmfs_fs_t, fs_info, vol_version, "Volume Version", uint),
   MEMBER2(vmfs_fs_t, fs_info, version, "Version", uint),
   MEMBER2(vmfs_fs_t, fs_info, label, "Label", string),
   MEMBER2(vmfs_fs_t, fs_info, mode, "Mode", fs_mode),
   MEMBER2(vmfs_fs_t, fs_info, uuid, "UUID", uuid),
   MEMBER2(vmfs_fs_t, fs_info, ctime, "Creation time", date),
   MEMBER2(vmfs_fs_t, fs_info, block_size, "Block size", size),
   MEMBER2(vmfs_fs_t, fs_info, subblock_size, "Subblock size", size),
   MEMBER2(vmfs_fs_t, fs_info, fdc_header_size, "FDC Header size", size),
   MEMBER2(vmfs_fs_t, fs_info, fdc_bitmap_count, "FDC Bitmap count", uint),
   { NULL, }
}};

struct var_struct vmfs_lvm = {
   struct_dump, {
   MEMBER2(vmfs_lvm_t, lvm_info, uuid, "UUID", uuid),
   MEMBER2(vmfs_lvm_t, lvm_info, size, "Size", size),
   MEMBER2(vmfs_lvm_t, lvm_info, blocks, "Blocks", uint),
   MEMBER2(vmfs_lvm_t, lvm_info, num_extents, "Num. Extents", uint),
   { NULL, }
}};

struct var_struct debugvmfs = {
   struct_dump, {
   SUBVAR(struct { char fs[0]; }, fs, vmfs_fs),
   SUBVAR(vmfs_fs_t, lvm, vmfs_lvm),
   SUBVAR(vmfs_fs_t, fbb, vmfs_bitmap),
   SUBVAR(vmfs_fs_t, fdc, vmfs_bitmap),
   SUBVAR(vmfs_fs_t, pbc, vmfs_bitmap),
   SUBVAR(vmfs_fs_t, sbc, vmfs_bitmap),
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

static char *get_value_bitmap_used(char *buf, void *value, short len)
{
   sprintf(buf, "%d", vmfs_bitmap_allocated_items((vmfs_bitmap_t *)value));
   return buf;
}

static char *get_value_bitmap_free(char *buf, void *value, short len)
{
   sprintf(buf, "%d", ((vmfs_bitmap_t *)value)->bmh.total_items -
                      vmfs_bitmap_allocated_items((vmfs_bitmap_t *)value));
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

static int struct_dump(struct var_struct *struct_def, void *value,
                       const char *name)
{
   char buf[256];
   struct var_member *member = struct_def->members;
   size_t len;

   if (!name || !*name) { /* name is empty, we dump all members */
      char format[16];
      sprintf(format, "%%%ds: %%s\n", longest_member_desc(struct_def));
      for (; member->member_name; member++)
         printf(format, member->description,
                member->get_value(buf, value + member->offset, member->length));
      return(1);
   }

   if (name[0] == '.')
      name++;

   len = strcspn(name, ".");

   if (name[len] != 0) { /* name contains a ., we search a sub var */
      strncpy(buf, name, len);
      buf[len] = 0;
      while(member->member_name && strcmp(member->member_name, buf))
         member++;
      if (member->get_value)
         return(0);
   } else
      while(member->member_name && strcmp(member->member_name, name))
         member++;

   if (!member->member_name)
      return(0);

   if (member->get_value) {
      printf("%s: %s\n", member->description,
             member->get_value(buf, value + member->offset, member->length));
      return(1);
   }

   value += member->offset;
   if (member->length)
      value = *(void **)value;
   return member->subvar->dump(member->subvar, value, name + len);
}

int vmfs_show_variable(const vmfs_fs_t *fs, const char *name)
{
   return debugvmfs.dump(&debugvmfs, (void *)fs, name) ? 0 : 1;
}
