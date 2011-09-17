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
   char *(*get_value)(char *buf, void *value, short len);
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
static char *get_value_mode(char *buf, void *value, short len);

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
   { NULL, }
};

static const struct var_member vmfs_fs[] = {
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
   if (!var)
      return;
   free_member(var->member, var->value);
   v = var->parent;
   free((void *)var);
   if (v && v != up_to)
      free_var(v, up_to);
}

static const char *find_closing_bracket(const char *str)
{
   size_t len = strcspn(str, "[]");
   switch (str[len]) {
   case '[': {
      const char *bracket = find_closing_bracket(str + len + 1);
      if (!bracket)
         return NULL;
      return find_closing_bracket(bracket + 1);
   }
   case ']':
      return str + len;
   default:
      return NULL;
   }
}

static int get_numeric_index(uint32_t *idx, const char *index);

static const struct var *resolve_var(const struct var *var, const char *name)
{
   size_t len;
   const struct var_member *m;
   char *index = NULL;
   struct var *res;

   if (!name || !var)
      return var;
   m = var->member->subvar;

   len = strcspn(name, ".[");

   if (len == 0)
      return NULL;

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
         end = find_closing_bracket(name + len + 1);

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
                const struct var_member *var = &root;
                void *idx_value = resolve_var(&var, index, root_obj);
                free(index);
                if (idx_value && var && var->get_value) {
                   index = malloc(256);
                   var->get_value(index, idx_value, var->length);
                } else
                   return NULL;
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

const char *hb_lock[] = {
   "unlocked",
   "write lock",
   "read lock",
};

static char *get_value_hb_lock(char *buf, void *value, short len)
{
   uint32_t lock = *((uint32_t *)value);
   if ((lock >= 0) && (lock <= 2))
      sprintf(buf, "%s", hb_lock[lock]);
   else
      sprintf(buf, "0x%x", lock);
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

static char *get_value_bitmap_item_status(char *buf, void *value, short len)
{
   struct vmfs_bitmap_item_ref *ref = (struct vmfs_bitmap_item_ref *) value;
   int used = vmfs_bitmap_get_item_status(&ref->bitmap->bmh, &ref->entry,
                                          ref->entry_idx, ref->item_idx);
   sprintf(buf, "%s", used ? "used" : "free");
   return buf;
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

   return buf;
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

static char *get_value_mode(char *buf, void *value, short len)
{
   char tbuf[64];
   uint32_t mode = *((uint32_t *)value);
   sprintf(buf, "%04o (%s)", mode, m_fmode_to_str(mode, tbuf));
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

int cmd_show(vmfs_dir_t *base_dir,int argc,char *argv[])
{
   char buf[256];
   current_dir = base_dir;
   const struct var root_var = { &root, (void *)vmfs_dir_get_fs(base_dir), NULL };
   const struct var *var = resolve_var(&root_var, argv[0]);
   int ret = 0;

   if (var) {
      const struct var_member *m = var->member;
      if (m->get_value) {
         printf("%s: %s\n", m->description, m->get_value(buf, var->value, m->length));
      } else if (m->subvar) {
         char format[16];
         const struct var_member *v;
         sprintf(format, "%%%ds: %%s\n", longest_member_desc(m->subvar));
         for (v = m->subvar; v->member_name; v++)
            if (v->description && v->get_value) {
               void *m_value = get_member(v, var->value, NULL);
               printf(format, v->description, v->get_value(buf, m_value, v->length));
               free_member(v, m_value);
            }
      } else
         ret = 1;
   } else
      ret = 1;
   free_var(var, &root_var);

   return ret;
}
