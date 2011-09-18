/*
 * vmfs-tools - Tools to access VMFS filesystems
 * Copyright (C) 2009,2011 Mike Hommey <mh@glandium.org>
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

struct var_member {
   const char *member_name;
   const char *description;
   const struct var_member *subvar;
   unsigned short offset;
   unsigned short length;
   void *(*get_member)(void *value, const char *index);
   void (*free_member)(void *value);
   char *(*get_value)(void *value, short len);
};

struct var {
   const struct var_member *member;
   void *value;
   const struct var *parent;
};

static void *get_bitmap_item(void *value, const char *index);
#define free_bitmap_item free
static void *get_bitmap_entry(void *value, const char *index);
#define free_bitmap_entry free
static void *get_lvm_extent(void *value, const char *index);
#define free_lvm_extent NULL
static void *get_blkid(void *value, const char *index);
#define free_blkid free
static void *get_dirent(void *value, const char *index);
#define free_dirent (void (*)(void *))vmfs_dir_close
static void *get_inode(void *value, const char *index);
#define free_inode (void (*)(void *))vmfs_inode_release
static void *get_lvm(void *value, const char *index);
#define free_lvm NULL

static char *get_value_none(void *value, short len);
static char *get_value_uint(void *value, short len);
static char *get_value_xint(void *value, short len);
static char *get_value_size(void *value, short len);
static char *get_value_string(void *value, short len);
static char *get_value_uuid(void *value, short len);
static char *get_value_date(void *value, short len);
static char *get_value_fs_mode(void *value, short len);
static char *get_value_hb_lock(void *value, short len);
static char *get_value_bitmap_used(void *value, short len);
static char *get_value_bitmap_free(void *value, short len);
static char *get_value_bitmap_item_status(void *value, short len);
static char *get_value_bitmap_item_dump(void *value, short len);
static char *get_value_vol_size(void *value, short len);
static char *get_value_blkid_item(void *value, short len);
static char *get_value_blkid_flags(void *value, short len);
static char *get_value_mode(void *value, short len);
static char *get_value_blocks(void *value, short len);

#define PTR(type, name) 0
#define FIELD(type, name) sizeof(((type *)0)->name)

#define MEMBER(type, name, desc, format) \
   { # name, desc, NULL, offsetof(type, name), \
     FIELD(type, name), NULL, NULL, get_value_ ## format }

#define MEMBER2(type, sub, name, desc, format) \
   { # name, desc, NULL, offsetof(type, sub.name), \
     FIELD(type, sub.name), NULL, NULL, get_value_ ## format }

#define VIRTUAL_MEMBER(type, name, desc, format) \
   { # name, desc, NULL, 0, sizeof(type), NULL, NULL, get_value_ ## format }

#define GET_SUBVAR(type, name, subvar, what) \
   { # name, NULL, subvar, 0, sizeof(type), get_ ## what, free_ ## what, NULL }

#define SUBVAR(type, name, subvar, kind) \
   { # name, NULL, subvar, offsetof(type, name), kind(type, name), NULL, NULL, NULL }

#define SUBVAR2(type, name, subvar, field_name, kind) \
   { # name, NULL, subvar, offsetof(type, field_name), kind(type, field_name), NULL, NULL, NULL }

static const struct var_member vmfs_metadata_hdr[] = {
   MEMBER(vmfs_metadata_hdr_t, magic, "Magic", xint),
   MEMBER(vmfs_metadata_hdr_t, pos, "Position", xint),
   MEMBER(vmfs_metadata_hdr_t, hb_pos, "HB Position", uint),
   MEMBER(vmfs_metadata_hdr_t, hb_lock, "HB Lock", hb_lock),
   MEMBER(vmfs_metadata_hdr_t, hb_uuid, "HB UUID", uuid),
   MEMBER(vmfs_metadata_hdr_t, hb_seq, "HB Sequence", uint),
   MEMBER(vmfs_metadata_hdr_t, obj_seq, "Obj Sequence", uint),
   MEMBER(vmfs_metadata_hdr_t, mtime, "MTime", uint),
   { NULL, }
};

struct vmfs_bitmap_item_ref {
   vmfs_bitmap_entry_t entry;
   vmfs_bitmap_t *bitmap;
   uint32_t entry_idx, item_idx;
};

static const struct var_member vmfs_bitmap_item[] = {
   VIRTUAL_MEMBER(struct vmfs_bitmap_item_ref, status, "Status", bitmap_item_status),
   VIRTUAL_MEMBER(struct vmfs_bitmap_item_ref, dump, NULL, bitmap_item_dump),
   { NULL, }
};

static const struct var_member vmfs_bitmap_entry[] = {
   MEMBER(vmfs_bitmap_entry_t, id, "Id", uint),
   MEMBER(vmfs_bitmap_entry_t, total, "Total items", uint),
   MEMBER(vmfs_bitmap_entry_t, free, "Free items", uint),
   MEMBER(vmfs_bitmap_entry_t, ffree, "First free", uint),
   SUBVAR(vmfs_inode_t, mdh, vmfs_metadata_hdr, FIELD),
   GET_SUBVAR(struct vmfs_bitmap_item_ref, item, vmfs_bitmap_item, bitmap_item),
   { NULL, }
};

static const struct var_member vmfs_bitmap[] = {
   MEMBER2(vmfs_bitmap_t, bmh, items_per_bitmap_entry,
           "Item per bitmap entry", uint),
   MEMBER2(vmfs_bitmap_t, bmh, bmp_entries_per_area,
           "Bitmap entries per area", uint),
   MEMBER2(vmfs_bitmap_t, bmh, hdr_size, "Header size", size),
   MEMBER2(vmfs_bitmap_t, bmh, data_size, "Data size", size),
   MEMBER2(vmfs_bitmap_t, bmh, area_size, "Area size", size),
   MEMBER2(vmfs_bitmap_t, bmh, area_count, "Area count", uint),
   MEMBER2(vmfs_bitmap_t, bmh, total_items, "Total items", uint),
   VIRTUAL_MEMBER(vmfs_bitmap_t, used_items, "Used items", bitmap_used),
   VIRTUAL_MEMBER(vmfs_bitmap_t, free_items, "Free items", bitmap_free),
   GET_SUBVAR(vmfs_bitmap_t, entry, vmfs_bitmap_entry, bitmap_entry),
   { NULL, }
};

static const struct var_member vmfs_volume[] = {
   MEMBER(vmfs_volume_t, device, "Device", string),
   MEMBER2(vmfs_volume_t, vol_info, uuid, "UUID", uuid),
   MEMBER2(vmfs_volume_t, vol_info, lun, "LUN", uint),
   MEMBER2(vmfs_volume_t, vol_info, version, "Version", uint),
   MEMBER2(vmfs_volume_t, vol_info, name, "Name", string),
   VIRTUAL_MEMBER(vmfs_volume_t, size, "Size", vol_size),
   MEMBER2(vmfs_volume_t, vol_info, num_segments, "Num. Segments", uint),
   MEMBER2(vmfs_volume_t, vol_info, first_segment, "First Segment", uint),
   MEMBER2(vmfs_volume_t, vol_info, last_segment, "Last Segment", uint),
   { NULL, }
};

static const struct var_member vmfs_lvm[] = {
   MEMBER2(vmfs_lvm_t, lvm_info, uuid, "UUID", uuid),
   MEMBER2(vmfs_lvm_t, lvm_info, size, "Size", size),
   MEMBER2(vmfs_lvm_t, lvm_info, blocks, "Blocks", uint),
   MEMBER2(vmfs_lvm_t, lvm_info, num_extents, "Num. Extents", uint),
   GET_SUBVAR(vmfs_lvm_t, extent, vmfs_volume, lvm_extent),
   { NULL, }
};

static const struct var_member blkid[] = {
   VIRTUAL_MEMBER(vmfs_block_info_t, item, "Referred Item", blkid_item),
   VIRTUAL_MEMBER(vmfs_block_info_t, flags, "Flags", blkid_flags),
   { NULL, }
};

static const struct var_member dirent[] = {
   MEMBER2(vmfs_dir_t, dirent, type, "Type", uint),
   MEMBER2(vmfs_dir_t, dirent, block_id, "Block ID", xint),
   MEMBER2(vmfs_dir_t, dirent, record_id, "Record ID", xint),
   MEMBER2(vmfs_dir_t, dirent, name, "Name", string),
   { NULL, }
};

static const struct var_member inode[] = {
   MEMBER(vmfs_inode_t, id, "ID", xint),
   MEMBER(vmfs_inode_t, id2, "ID2", xint),
   MEMBER(vmfs_inode_t, nlink, "Links", uint),
   MEMBER(vmfs_inode_t, type, "Type", uint),
   MEMBER(vmfs_inode_t, flags, "Flags", uint),
   MEMBER(vmfs_inode_t, size, "Size", size),
   MEMBER(vmfs_inode_t, blk_size, "Block size", size),
   MEMBER(vmfs_inode_t, blk_count, "Block count", uint),
   MEMBER(vmfs_inode_t, uid, "UID", uint),
   MEMBER(vmfs_inode_t, gid, "GID", uint),
   MEMBER(vmfs_inode_t, mode, "Mode", mode),
   MEMBER(vmfs_inode_t, zla, "ZLA", uint),
   MEMBER(vmfs_inode_t, tbz, "TBZ", uint),
   MEMBER(vmfs_inode_t, cow, "COW", uint),
   MEMBER(vmfs_inode_t, atime, "Access Time", date),
   MEMBER(vmfs_inode_t, mtime, "Modify Time", date),
   MEMBER(vmfs_inode_t, ctime, "Change Time", date),
   MEMBER(vmfs_inode_t, rdm_id, "RDM ID", xint),
   SUBVAR(vmfs_inode_t, mdh, vmfs_metadata_hdr, FIELD),
   VIRTUAL_MEMBER(vmfs_inode_t, blocks, NULL, blocks),
   { NULL, }
};

static const struct var_member vmfs_fs[] = {
   GET_SUBVAR(vmfs_fs_t, lvm, vmfs_lvm, lvm),
   SUBVAR2(vmfs_fs_t, lvm, vmfs_lvm, dev, PTR),
   SUBVAR(vmfs_fs_t, fbb, vmfs_bitmap, PTR),
   SUBVAR(vmfs_fs_t, fdc, vmfs_bitmap, PTR),
   SUBVAR(vmfs_fs_t, pbc, vmfs_bitmap, PTR),
   SUBVAR(vmfs_fs_t, sbc, vmfs_bitmap, PTR),
   GET_SUBVAR(vmfs_fs_t, blkid, blkid, blkid),
   GET_SUBVAR(vmfs_fs_t, dirent, dirent, dirent),
   GET_SUBVAR(vmfs_fs_t, inode, inode, inode),
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
};

static const struct var_member root = {
   NULL, NULL, vmfs_fs, 0, sizeof(vmfs_fs_t), NULL, NULL, NULL
};

static void *get_member(const struct var_member *member, void *value, const char *index)
{
   void *result = &((char *) value)[member->offset];
   if (member->length == 0)
      result = *(void**)result;
   if (member->get_member)
      result = member->get_member(result, index);
   return result;
}

static void free_member(const struct var_member *member, void *value)
{
   if (!value)
      return;
   if (member->free_member)
      member->free_member(value);
}

static void free_var(const struct var *var, const struct var *up_to)
{
   const struct var *v;
   if (!var || var == up_to)
      return;
   free_member(var->member, var->value);
   v = var->parent;
   free((void *)var);
   if (v && v != up_to)
      free_var(v, up_to);
}

static const char *find_closing_thing(const char* openclose, const char *str)
{
   size_t len = strcspn(str, openclose);
   if (str[len] == openclose[0]) {
      const char *thing = find_closing_thing(openclose, str + len + 1);
      if (!thing)
         return NULL;
      return find_closing_thing(openclose, thing + 1);
   } else if (str[len] == openclose[1])
      return str + len;
   return NULL;
}

static int get_numeric_index(uint32_t *idx, const char *index);

static const struct var *resolve_var(const struct var *var, const char *name)
{
   /* TODO: this function really needs some simplification/splitting */
   size_t len;
   const struct var_member *m;
   char *index = NULL;
   struct var *res;

   if (!name || !var)
      return var;

   len = strcspn(name, ".[(");

   if (name[len] == '(') {
      const struct var *idx_var;
      const char *end;
      if (len != 0 || var->parent)
         return NULL;
      if (!(end = find_closing_thing("()", name + 1)))
         return NULL;
      len = end - name - 1;
      index = malloc(len + 1);
      strncpy(index, name + 1, len);
      index[len] = 0;
      len += 2;
      idx_var = resolve_var(var, index);
      free(index);
      if (idx_var && idx_var->member->get_value) {
         index = idx_var->member->get_value(idx_var->value, idx_var->member->length);
         free_var(idx_var, var);
         var = resolve_var(var, index);
         free(index);
         index = NULL;
         name += len;
         if (*name == '.')
            name++;
         len = strcspn(name, ".[");
      } else
         return NULL;
   }

   if (!name[0])
      return var;

   if (len == 0)
      return NULL;

   m = var->member->subvar;

   while (m->member_name && !(!strncmp(m->member_name, name, len) && m->member_name[len] == 0))
      m++;

   if (!m->member_name)
      return NULL;

   if (name[len] == '[') {
      size_t len2 = len + 1;
      const char *end;
      int is_str = 0;

      if (name[len2] == '"') {
         end = strchr(name + len2 + 1, '"');
         if (end[1] != ']')
            return NULL;
         is_str = 1;
         len++;
      } else
         end = find_closing_thing("[]", name + len + 1);

      if (end) {
          len2 = end - name - 1;
          index = malloc(len2 - len + 1);
          strncpy(index, name + len + 1, len2 - len);
          index[len2 - len] = 0;
          len = len2 + 2;
          if (is_str) {
             len++;
          } else {
             uint32_t idx;
             if (!get_numeric_index(&idx, index)) {
                const struct var *root_var, *idx_var;
                for (root_var = var; root_var->parent; root_var = root_var->parent);
                idx_var = resolve_var(root_var, index);
                free(index);
                if (idx_var && idx_var->member->get_value) {
                   index = idx_var->member->get_value(idx_var->value, idx_var->member->length);
                } else
                   return NULL;
                free_var(idx_var, root_var);
             }
          }
      } else
          return NULL;
   }

   res = malloc(sizeof(struct var));
   res->value = get_member(m, var->value, index);
   free(index);
   res->member = m;
   res->parent = var;
   if (name[len]) {
      const struct var *r = resolve_var(res, name + len + 1);
      if (!r)
         free(res);
      return r;
   }

   return res;
}

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

static char *human_readable_size(uint64_t size)
{
   char buf[256];
   int scale = 0;
   for (scale = 0; (size >> scale) >= 1024; scale += 10);

   if (size & ((1L << scale) - 1))
      sprintf(buf, "%.2f%s", (float) size / (1L << scale), units[scale / 10]);
   else
      sprintf(buf, "%"PRIu64"%s", size >> scale, units[scale / 10]);

   return strdup(buf);
}

static char *get_value_uint(void *value, short len)
{
   char buf[32];
   switch (len) {
   case 4:
      sprintf(buf, "%" PRIu32, *((uint32_t *)value));
      return strdup(buf);
   case 8:
      sprintf(buf, "%" PRIu64, *((uint64_t *)value));
      return strdup(buf);
   }
   return get_value_none(value, len);
}

static char *get_value_xint(void *value, short len)
{
   char buf[32];
   switch (len) {
   case 4:
      sprintf(buf, "0x%" PRIx32, *((uint32_t *)value));
      return strdup(buf);
   case 8:
      sprintf(buf, "0x%" PRIx64, *((uint64_t *)value));
      return strdup(buf);
   }
   return get_value_none(value, len);
}

static char *get_value_size(void *value, short len)
{
   switch (len) {
   case 4:
      return human_readable_size(*((uint32_t *)value));
   case 8:
      return human_readable_size(*((uint64_t *)value));
   }
   return get_value_none(value, len);
}

static char *get_value_string(void *value, short len)
{
   if (len == sizeof(void *))
      return strdup(*((char **)value));
   return strdup((char *)value);
}

static char *get_value_uuid(void *value, short len)
{
   char *buf = malloc(36);
   return m_uuid_to_str((u_char *)value,buf);
}

static char *get_value_date(void *value, short len)
{
   char buf[256];
   return strdup(m_ctime((time_t *)(uint32_t *)value, buf, 256));
}

static char *get_value_fs_mode(void *value, short len)
{
   return strdup(vmfs_fs_mode_to_str(*((uint32_t *)value)));
}

const char *hb_lock[] = {
   "unlocked",
   "write lock",
   "read lock",
};

static char *get_value_hb_lock(void *value, short len)
{
   uint32_t lock = *((uint32_t *)value);
   if ((lock >= 0) && (lock <= 2))
      return strdup(hb_lock[lock]);
   else {
      char buf[256];
      sprintf(buf, "0x%x", lock);
      return strdup(buf);
   }
}

static char *get_value_none(void *value, short len)
{
   return strdup("Don't know how to display");
}

static char *get_value_bitmap_used(void *value, short len)
{
   char buf[32];
   sprintf(buf, "%d", vmfs_bitmap_allocated_items((vmfs_bitmap_t *)value));
   return strdup(buf);
}

static char *get_value_bitmap_free(void *value, short len)
{
   char buf[32];
   sprintf(buf, "%d", ((vmfs_bitmap_t *)value)->bmh.total_items -
                      vmfs_bitmap_allocated_items((vmfs_bitmap_t *)value));
   return strdup(buf);
}

static char *get_value_vol_size(void *value, short len)
{
   return human_readable_size(
                   (uint64_t)(((vmfs_volume_t *)value)->vol_info.size) * 256);
}

static int longest_member_desc(const struct var_member *members)
{
   int len = 0, curlen;
   for (; members->member_name; members++) {
      if (!members->description)
         continue;
      curlen = strlen(members->description);
      if (curlen > len)
         len = curlen;
   }
   return len;
}

static int get_numeric_index(uint32_t *idx, const char *index)
{
   char *c;
   unsigned long ret;

   if (!idx)
      return 0;

   ret = strtoul(index, &c, 0);

   if (*c || ret > 0xffffffff)
      return 0;

   *idx = ret;
   return 1;
}

static void *get_lvm_extent(void *value, const char *index)
{
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)value;
   uint32_t idx;

   if (!index ||
       !get_numeric_index(&idx, index) ||
       idx >= lvm->lvm_info.num_extents)
      return NULL;

   return lvm->extents[idx];
}

static void *get_bitmap_entry(void *value, const char *index)
{
   struct vmfs_bitmap_item_ref *ref =
      calloc(1, sizeof(struct vmfs_bitmap_item_ref));

   if (!index)
      return NULL;

   ref->bitmap = (vmfs_bitmap_t *)value;

   if (!get_numeric_index(&ref->entry_idx, index))
      return NULL;

   if (ref->entry_idx >= ref->bitmap->bmh.bmp_entries_per_area *
                         ref->bitmap->bmh.area_count)
      return NULL;
   vmfs_bitmap_get_entry(ref->bitmap, ref->entry_idx, 0, &ref->entry);

   return ref;
}

static char *get_value_bitmap_item_status(void *value, short len)
{
   struct vmfs_bitmap_item_ref *ref = (struct vmfs_bitmap_item_ref *) value;
   int used = vmfs_bitmap_get_item_status(&ref->bitmap->bmh, &ref->entry,
                                          ref->entry_idx, ref->item_idx);
   return strdup(used ? "used" : "free");
}

void dump_line(char *buf, uint32_t offset, u_char *data, size_t len)
{
   u_char b[17];
   size_t i;
   sprintf(buf, "%08x  ", offset);
   buf += 10;
   for (i = 0; i < len; i++) {
      sprintf(buf, (i == 7) ? "%02x  " : "%02x ", data[i]);
      buf += (i == 7) ? 4 : 3;
      b[i] = ((data[i] > 31) && (data[i] < 127)) ? data[i] : '.';
   }
   b[i] = 0;
   for (; i < 16; i++) {
      sprintf(buf, (i == 7) ? "    " : "   ");
      buf += (i == 7) ? 4 : 3;
   }
   sprintf(buf, " |%s|\n", b);
}


static char *get_value_bitmap_item_dump(void *value, short len)
{
   struct vmfs_bitmap_item_ref *ref = (struct vmfs_bitmap_item_ref *) value;
   uint32_t off = 0, size = ref->bitmap->bmh.data_size;
   u_char *data;
   char *dump, *buf;
   bool is_fbb = false;

   if (size == 0) {
      /* If bitmap data size is 0, it is very likely the bitmap is fbb.
         But just in case, we make sure it is */
      if (ref->bitmap->f->inode->fs->fbb != ref->bitmap)
         return NULL;
      size = vmfs_fs_get_blocksize(ref->bitmap->f->inode->fs);
      is_fbb = true;
   }

   data = iobuffer_alloc(size);
   buf = dump = malloc(79 * (size + 15) / 16);

   if (is_fbb)
      vmfs_fs_read(ref->bitmap->f->inode->fs,
                   ref->entry_idx * ref->bitmap->bmh.items_per_bitmap_entry + ref->item_idx,
                   0, data, size);
   else
      vmfs_bitmap_get_item(ref->bitmap, ref->entry_idx, ref->item_idx, data);

   while (size >= 16) {
      dump_line(buf, off, data + off, 16);
      size -= 16;
      off += 16;
      buf += 79;
   }
   if (size > 0)
      dump_line(buf, off, data + off, size);

   free(data);
   return dump;
}

static void *get_bitmap_item(void *value, const char *index)
{
   struct vmfs_bitmap_item_ref *ref =
      malloc(sizeof(struct vmfs_bitmap_item_ref));
   /* This is not nice, but we need to copy the struct for resolve_var to
    * be happy about it */
   memcpy(ref, value, sizeof(struct vmfs_bitmap_item_ref));

   if (!index)
      return NULL;

   if (!get_numeric_index(&ref->item_idx, index))
      return NULL;

   if (ref->item_idx >= ref->bitmap->bmh.items_per_bitmap_entry)
      return NULL;

   return ref;
}

static const char *bitmaps[] = { "fbb", "sbc", "pbc", "fdc" };

static char *get_value_blkid_item(void *value, short len)
{
   char buf[256];
   vmfs_block_info_t *info = (vmfs_block_info_t *)value;
   sprintf(buf, "%s.entry[%d].item[%d]", bitmaps[info->type - 1],
                                         info->entry, info->item);
   return strdup(buf);
}

static char *get_value_blkid_flags(void *value, short len)
{
   char buf[256];
   vmfs_block_info_t *info = (vmfs_block_info_t *)value;
   int more_than_one = 0;

   if (sprintf(buf, "0x%x (", info->flags) <= 0)
      return NULL;

   if (info->flags & VMFS_BLK_FB_TBZ_FLAG) {
      strcat(buf, "tbz");
      more_than_one = 1;
   }

   if (info->flags & ~VMFS_BLK_FB_TBZ_FLAG) {
      if (more_than_one)
         strcat(buf, ", ");
      strcat(buf, "unknown");
   }

   if (!info->flags)
      strcat(buf, "none");

   strcat(buf, ")");

   return strdup(buf);
}

static void *get_blkid(void *value, const char *index)
{
   vmfs_block_info_t *info;
   uint32_t idx;

   if (!index ||
       !get_numeric_index(&idx, index))
      return NULL;

   info = calloc(1, sizeof(vmfs_block_info_t));
   if (vmfs_block_get_info(idx, info) == -1) {
      free(info);
      return NULL;
   }
   /* Normalize entry and item for fbb */
   if (info->type == VMFS_BLK_TYPE_FB) {
      vmfs_fs_t *fs = (vmfs_fs_t *)value;
      info->entry = info->item / fs->fbb->bmh.items_per_bitmap_entry;
      info->item = info->item % fs->fbb->bmh.items_per_bitmap_entry;
   }
   return info;
}

static vmfs_dir_t *current_dir = NULL;

static void *get_dirent(void *value, const char *index)
{
   vmfs_dir_t *dir;
   char *bname, *dname;

   if (!index)
      return NULL;

   bname = m_basename(index);
   dname = m_dirname(index);

   if (!(dir = vmfs_dir_open_at(current_dir,dname)))
      return NULL;
   if (!vmfs_dir_lookup(dir,bname))
      return NULL;

   free(bname);
   free(dname);
   return dir;
}

static char *get_value_mode(void *value, short len)
{
   char *buf = malloc(18);
   uint32_t mode = *((uint32_t *)value);
   sprintf(buf, "%04o (", mode);
   m_fmode_to_str(mode, buf + 6);
   buf[16] = ')';
   buf[17] = 0;
   return buf;
}

static char *get_value_blocks(void *value, short len)
{
   char *buf, *b;
   int i, num = FIELD(vmfs_inode_t, blocks) / sizeof(uint32_t);

   vmfs_inode_t *inode = (vmfs_inode_t *) value;
   while (num > 0 && !inode->blocks[num - 1])
      num--;

   b = buf = malloc(sizeof("0x00000000") * num + 1);
   for (i = 0; i < num; i++) {
      sprintf(b, "0x%08x%c", inode->blocks[i], (i + 1) % 4 ? ' ' : '\n');
      b += sizeof("0x00000000");
   }

   return buf;
}

/* Defined in debugvmfs.c */
vmfs_file_t *vmfs_file_open_from_filespec(vmfs_dir_t *base_dir,
                                          const char *filespec);

static void *get_inode(void *value, const char *index)
{
   vmfs_file_t *file;
   vmfs_inode_t *inode;

   if (!index)
      return NULL;

   if (!(file = vmfs_file_open_from_filespec(current_dir,index)))
      return NULL;

   /* Awkward, need better API */
   inode = vmfs_inode_acquire(vmfs_dir_get_fs(current_dir), file->inode->id);
   vmfs_file_close(file);
   return inode;
}

static void *get_lvm(void *value, const char *index)
{
   vmfs_fs_t *fs = (vmfs_fs_t *) value;
   if (vmfs_device_is_lvm(fs->dev))
      return fs->dev;

   return NULL;
}

int cmd_show(vmfs_dir_t *base_dir,int argc,char *argv[])
{
   current_dir = base_dir;
   const struct var root_var = { &root, (void *)vmfs_dir_get_fs(base_dir), NULL };
   const struct var *var = resolve_var(&root_var, argv[0]);
   int ret = 0;

   if (var) {
      const struct var_member *m = var->member;
      if (m->get_value) {
         char *str = m->get_value(var->value, m->length);
         if (str) {
            if (m->description)
               printf("%s: %s\n", m->description, str);
            else
               printf("%s\n", str);
            free(str);
         }
      } else if (m->subvar) {
         char format[16];
         const struct var_member *v;
         sprintf(format, "%%%ds: %%s\n", longest_member_desc(m->subvar));
         for (v = m->subvar; v->member_name; v++)
            if (v->description && v->get_value) {
               void *m_value = get_member(v, var->value, NULL);
               char *str = v->get_value(m_value, v->length);
               printf(format, v->description, str);
               free(str);
               free_member(v, m_value);
            }
      } else
         ret = 1;
   } else
      ret = 1;
   free_var(var, &root_var);

   return ret;
}
