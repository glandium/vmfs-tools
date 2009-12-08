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
#include <stdlib.h>
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

static int bitmap_items_dump(struct var_struct *struct_def, void *value,
                               const char *name);
static int bitmap_entries_dump(struct var_struct *struct_def, void *value,
                               const char *name);
static int lvm_extents_dump(struct var_struct *struct_def, void *value,
                            const char *name);
static int blkid_dump(struct var_struct *struct_def, void *value,
                      const char *name);
static int struct_dump(struct var_struct *struct_def, void *value,
                       const char *name);

static char *get_value_none(char *buf, void *value, short len);
static char *get_value_uint(char *buf, void *value, short len);
static char *get_value_xint(char *buf, void *value, short len);
static char *get_value_size(char *buf, void *value, short len);
static char *get_value_string(char *buf, void *value, short len);
static char *get_value_uuid(char *buf, void *value, short len);
static char *get_value_date(char *buf, void *value, short len);
static char *get_value_fs_mode(char *buf, void *value, short len);
static char *get_value_hb_lock(char *buf, void *value, short len);
static char *get_value_bitmap_used(char *buf, void *value, short len);
static char *get_value_bitmap_free(char *buf, void *value, short len);
static char *get_value_bitmap_item_status(char *buf, void *value, short len);
static char *get_value_vol_size(char *buf, void *value, short len);
static char *get_value_blkid_item(char *buf, void *value, short len);
static char *get_value_blkid_flags(char *buf, void *value, short len);

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

#define SELF_SUBVAR(name, struct_def) \
   SUBVAR(struct { char name[0]; }, name, struct_def)

#define ARRAY_MEMBER(struct_def) \
   { NULL, { subvar: &struct_def }, 0, 0, NULL }

struct var_struct vmfs_metadata_hdr = {
   struct_dump, {
   MEMBER(vmfs_metadata_hdr_t, magic, "Magic", xint),
   MEMBER(vmfs_metadata_hdr_t, pos, "Position", xint),
   MEMBER(vmfs_metadata_hdr_t, hb_pos, "HB Position", uint),
   MEMBER(vmfs_metadata_hdr_t, hb_lock, "HB Lock", hb_lock),
   MEMBER(vmfs_metadata_hdr_t, hb_uuid, "HB UUID", uuid),
   MEMBER(vmfs_metadata_hdr_t, hb_seq, "HB Sequence", uint),
   MEMBER(vmfs_metadata_hdr_t, obj_seq, "Obj Sequence", uint),
   MEMBER(vmfs_metadata_hdr_t, mtime, "MTime", uint),
   { NULL, }
}};

struct var_struct vmfs_bitmap_item = {
   struct_dump, {
   VIRTUAL_MEMBER(status, "Status", bitmap_item_status),
   { NULL, }
}};

struct var_struct vmfs_bitmap_items = {
   bitmap_items_dump, {
   ARRAY_MEMBER(vmfs_bitmap_item),
   { NULL, }
}};

struct var_struct vmfs_bitmap_entry = {
   struct_dump, {
   MEMBER(vmfs_bitmap_entry_t, id, "Id", uint),
   MEMBER(vmfs_bitmap_entry_t, total, "Total items", uint),
   MEMBER(vmfs_bitmap_entry_t, free, "Free items", uint),
   MEMBER(vmfs_bitmap_entry_t, ffree, "First free", uint),
   SELF_SUBVAR(mdh, vmfs_metadata_hdr),
   SELF_SUBVAR(item, vmfs_bitmap_items),
   { NULL, }
}};

struct var_struct vmfs_bitmap_entries = {
   bitmap_entries_dump, {
   ARRAY_MEMBER(vmfs_bitmap_entry),
   { NULL, }
}};

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
   SELF_SUBVAR(entry, vmfs_bitmap_entries),
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

struct var_struct vmfs_volume = {
   struct_dump, {
   MEMBER(vmfs_volume_t, device, "Device", string),
   MEMBER2(vmfs_volume_t, vol_info, uuid, "UUID", uuid),
   MEMBER2(vmfs_volume_t, vol_info, lun, "LUN", uint),
   MEMBER2(vmfs_volume_t, vol_info, version, "Version", uint),
   MEMBER2(vmfs_volume_t, vol_info, name, "Name", string),
   VIRTUAL_MEMBER(size, "Size", vol_size),
   MEMBER2(vmfs_volume_t, vol_info, num_segments, "Num. Segments", uint),
   MEMBER2(vmfs_volume_t, vol_info, first_segment, "First Segment", uint),
   MEMBER2(vmfs_volume_t, vol_info, last_segment, "Last Segment", uint),
   { NULL, }
}};

struct var_struct vmfs_lvm_extent = {
   lvm_extents_dump, {
   ARRAY_MEMBER(vmfs_volume),
   { NULL, }
}};

struct var_struct vmfs_lvm = {
   struct_dump, {
   MEMBER2(vmfs_lvm_t, lvm_info, uuid, "UUID", uuid),
   MEMBER2(vmfs_lvm_t, lvm_info, size, "Size", size),
   MEMBER2(vmfs_lvm_t, lvm_info, blocks, "Blocks", uint),
   MEMBER2(vmfs_lvm_t, lvm_info, num_extents, "Num. Extents", uint),
   SELF_SUBVAR(extent, vmfs_lvm_extent),
   { NULL, }
}};

struct var_struct blkid = {
   struct_dump, {
   VIRTUAL_MEMBER(item, "Referred Item", blkid_item),
   VIRTUAL_MEMBER(flags, "Flags", blkid_flags),
   { NULL, }
}};

struct var_struct blkid_array = {
   blkid_dump, {
   ARRAY_MEMBER(blkid),
   { NULL, }
}};

struct var_struct debugvmfs = {
   struct_dump, {
   SELF_SUBVAR(fs, vmfs_fs),
   SUBVAR(vmfs_fs_t, lvm, vmfs_lvm),
   SUBVAR(vmfs_fs_t, fbb, vmfs_bitmap),
   SUBVAR(vmfs_fs_t, fdc, vmfs_bitmap),
   SUBVAR(vmfs_fs_t, pbc, vmfs_bitmap),
   SUBVAR(vmfs_fs_t, sbc, vmfs_bitmap),
   SELF_SUBVAR(blkid, blkid_array),
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

static char *get_value_xint(char *buf, void *value, short len)
{
   switch (len) {
   case 4:
      sprintf(buf, "0x%" PRIx32, *((uint32_t *)value));
      return buf;
   case 8:
      sprintf(buf, "0x%" PRIx64, *((uint64_t *)value));
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
   if (len == sizeof(void *))
      strcpy(buf, *((char **)value));
   else
      strcpy(buf, (char *)value);
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

static char *get_value_hb_lock(char *buf, void *value, short len)
{
   sprintf(buf, "%s", *((uint32_t *)value) ? "locked" : "unlocked");
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

static char *get_value_vol_size(char *buf, void *value, short len)
{
   return human_readable_size(buf,
                   (uint64_t)(((vmfs_volume_t *)value)->vol_info.size) * 256);
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
         if (member->get_value)
            printf(format, member->description,
                member->get_value(buf, value + member->offset, member->length));
      return(1);
   }

   if (name[0] == '.')
      name++;

   len = strcspn(name, ".[");

   if (name[len] != 0) { /* name contains a . or [, we search a sub var */
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

static int get_array_index(char *idx_str, const char **name)
{
   char *current = idx_str;
   const char *end = *name, *next;
   if (*end != '[')
      return(0);
   do {
      next = index(end + 1, ']');
      if (next) {
         strncpy(current, end + 1, next - end - 1);
         current += next - end - 1;
         end = next;
      } else
         return(0);
   } while (*(end - 1) == '\\');
   *current = 0;
   *name = end + 1;
   return(1);
}

static int get_numeric_index(uint32_t *idx, const char **name)
{
   char idx_str[1024], *c;
   unsigned long long_idx;
   if (!get_array_index(idx_str, name))
      return(0);
   long_idx = strtoul(idx_str, &c, 0);
   if (*c || (long_idx > (uint32_t)-1))
      return(0);
   *idx = long_idx;
   return(1);
}

static int lvm_extents_dump(struct var_struct *struct_def, void *value,
                            const char *name)
{
   uint32_t idx;
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)value;
   if (!get_numeric_index(&idx, &name))
      return(0);

   if (idx >= lvm->lvm_info.num_extents)
      return(0);

   return struct_def->members[0].subvar->dump(struct_def->members[0].subvar,
                                              lvm->extents[idx], name);
}

struct vmfs_bitmap_item_ref {
   vmfs_bitmap_entry_t entry;
   vmfs_bitmap_t *bitmap;
   uint32_t entry_idx, item_idx;
};

static int bitmap_entries_dump(struct var_struct *struct_def, void *value,
                               const char *name)
{
   struct vmfs_bitmap_item_ref ref = { { { 0, } } };
   ref.bitmap = (vmfs_bitmap_t *)value;
   if (!get_numeric_index(&ref.entry_idx, &name))
      return(0);

   if (ref.entry_idx >= ref.bitmap->bmh.bmp_entries_per_area *
                        ref.bitmap->bmh.area_count)
      return(0);

   vmfs_bitmap_get_entry(ref.bitmap, ref.entry_idx, 0, &ref.entry);
   return struct_def->members[0].subvar->dump(struct_def->members[0].subvar,
                                              &ref, name);
}

static char *get_value_bitmap_item_status(char *buf, void *value, short len)
{
   struct vmfs_bitmap_item_ref *ref = (struct vmfs_bitmap_item_ref *) value;
   int used = vmfs_bitmap_get_item_status(&ref->bitmap->bmh, value,
                                          ref->entry_idx, ref->item_idx);
   sprintf(buf, "%s", used ? "used" : "free");
   return buf;
}

static int bitmap_items_dump(struct var_struct *struct_def, void *value,
                               const char *name)
{
   struct vmfs_bitmap_item_ref *ref = (struct vmfs_bitmap_item_ref *) value;
   if (!get_numeric_index(&ref->item_idx, &name))
      return(0);

   if (ref->item_idx >= ref->bitmap->bmh.items_per_bitmap_entry)
      return(0);

   return struct_def->members[0].subvar->dump(struct_def->members[0].subvar,
                                              ref, name);
}

static const char *bitmaps[] = { "fbb", "sbc", "pbc", "fdc" };

static char *get_value_blkid_item(char *buf, void *value, short len)
{
   vmfs_block_info_t *info = (vmfs_block_info_t *)value;
   sprintf(buf, "%s.entry[%d].item[%d]", bitmaps[info->type - 1],
                                         info->entry, info->item);
   return buf;
}

static char *get_value_blkid_flags(char *buf, void *value, short len)
{
   vmfs_block_info_t *info = (vmfs_block_info_t *)value;
   if (info->flags)
      sprintf(buf, "0x%x (%s)", info->flags,
                                info->flags == VMFS_BLK_FB_TBZ_FLAG ? "tbz" :
                                info->flags & VMFS_BLK_FB_TBZ_FLAG ? "tbz, unknown" :
                                "unknown");
   else
      sprintf(buf, "none");
   return buf;
}

static int blkid_dump(struct var_struct *struct_def, void *value,
                      const char *name)
{
   uint32_t blkid;
   vmfs_block_info_t info;
   if (!get_numeric_index(&blkid, &name))
      return(0);
   if (vmfs_block_get_info(blkid, &info) == -1)
      return(0);
   return struct_def->members[0].subvar->dump(struct_def->members[0].subvar,
                                              &info, name);
}

static vmfs_dir_t *current_dir = NULL;

int cmd_show(vmfs_dir_t *base_dir,int argc,char *argv[])
{
   current_dir = base_dir;
   return debugvmfs.dump(&debugvmfs, (void *)vmfs_dir_get_fs(base_dir),
                         argv[0]) ? 0 : 1;
}
