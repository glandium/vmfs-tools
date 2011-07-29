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

#include <libgen.h>
#include <stdlib.h>
#include <ctype.h>
#include "readcmd.h"
#include "vmfs.h"

/* Until a better API is defined */
int vmfs_bmh_write(const vmfs_bitmap_header_t *bmh,u_char *buf);

static int cmd_remove(vmfs_fs_t *fs,int argc,char *argv[])
{
   /* Dangerous */
   vmfs_lvm_t *lvm = (vmfs_lvm_t *)fs->dev;
   vmfs_volume_t *extent;
   DECL_ALIGNED_BUFFER(buf,512);
   vmfs_bitmap_entry_t entry;
   uint32_t i, blocks_per_segment, items_per_area,
            items_in_last_entry, old_area_count;

   fprintf(stderr, "Extents removal is experimental ! Use at your own risk !\n");
   while (1) {
      char *answer = local_readline("Are you sure you want to go further? [y/N] ");
      char a = answer[0];
      char null = answer[1];
      free(answer);
      if ((a == 0) || (tolower(a) == 'n') && (null == 0))
         return(2);
      if ((tolower(a) == 'y') && (null == 0))
         break;
   }
   if (lvm->lvm_info.num_extents == 1) {
      fprintf(stderr, "Can't remove an extent when there is only one\n");
      return(1);
   }

   /* Get last extent */
   extent = lvm->extents[lvm->loaded_extents - 1];

   /* Check whether data blocks are used on the last extents */
   blocks_per_segment = VMFS_LVM_SEGMENT_SIZE / vmfs_fs_get_blocksize(fs);
   for (i = extent->vol_info.first_segment * blocks_per_segment;
        i < extent->vol_info.last_segment * blocks_per_segment;
        i++)
      if (vmfs_block_get_status(fs, VMFS_BLK_FB_BUILD(i))) {
         fprintf(stderr, "There is data on the last extent ; can't remove it\n");
         return(1);
      }

   /* At filesystem level, only the FBB needs to be downsized */
   fs->fbb->bmh.total_items -= extent->vol_info.num_segments * blocks_per_segment;
   items_per_area = fs->fbb->bmh.items_per_bitmap_entry *
                    fs->fbb->bmh.bmp_entries_per_area;
   old_area_count = fs->fbb->bmh.area_count;
   fs->fbb->bmh.area_count = (fs->fbb->bmh.total_items + items_per_area - 1) /
                             items_per_area;

   memset(buf, 0, buf_len);
   vmfs_bmh_write(&fs->fbb->bmh, buf);
   /* TODO: Error handling */
   vmfs_file_pwrite(fs->fbb->f, buf, buf_len, 0);

   /* Adjust new last entry */
   if ((items_in_last_entry =
        fs->fbb->bmh.total_items % fs->fbb->bmh.items_per_bitmap_entry)) {
      vmfs_bitmap_get_entry(fs->fbb, 0, fs->fbb->bmh.total_items, &entry);
      entry.free -= entry.total - items_in_last_entry;
      entry.total = items_in_last_entry;
      if (entry.ffree > entry.total)
         entry.ffree = 0;
      /* Adjust bitmap in last entry */
      if (items_in_last_entry % 8)
         entry.bitmap[items_in_last_entry / 8] &=
            0xff << (8 - (items_in_last_entry % 8));
      memset(&entry.bitmap[(items_in_last_entry + 7) / 8], 0,
         (fs->fbb->bmh.items_per_bitmap_entry - items_in_last_entry - 7) / 8);
      vmfs_bme_update(fs, &entry);
   }
   /* Truncate the fbb file depending on the new area count */
   if (old_area_count != fs->fbb->bmh.area_count)
      vmfs_file_truncate(fs->fbb->f, fs->fbb->bmh.hdr_size +
                            fs->fbb->bmh.area_count * fs->fbb->bmh.area_size);
   /* Adjust entries after the new last one */
   for (i = fs->fbb->bmh.total_items + (items_in_last_entry ?
           (fs->fbb->bmh.items_per_bitmap_entry - items_in_last_entry) : 0);
        i < fs->fbb->bmh.area_count * items_per_area;
        i += fs->fbb->bmh.items_per_bitmap_entry) {
      uint64_t pos;
      vmfs_bitmap_get_entry(fs->fbb, 0, i, &entry);
      pos = entry.mdh.pos;
      memset(&entry, 0, sizeof(entry));
      vmfs_device_write(fs->dev, pos, (u_char *)&entry, sizeof(entry));
   }

   /* At LVM level, all extents need to have the LVM information updated */
   for (i = 0; i < lvm->loaded_extents - 1; i++) {
      lvm->extents[i]->vol_info.blocks -= extent->vol_info.num_segments + 1;
      lvm->extents[i]->vol_info.num_extents--;
      lvm->extents[i]->vol_info.lvm_size =
         (lvm->extents[i]->vol_info.blocks -
            lvm->extents[i]->vol_info.num_extents) * VMFS_LVM_SEGMENT_SIZE;

      /* Until there is an API for that, do it by hand */
      m_pread(lvm->extents[i]->fd,buf,buf_len,
              lvm->extents[i]->vmfs_base + VMFS_LVMINFO_OFFSET);
      write_le64(buf, VMFS_LVMINFO_OFS_SIZE - VMFS_LVMINFO_OFFSET,
         lvm->extents[i]->vol_info.lvm_size);
      write_le64(buf, VMFS_LVMINFO_OFS_BLKS - VMFS_LVMINFO_OFFSET,
         lvm->extents[i]->vol_info.blocks);
      write_le32(buf, VMFS_LVMINFO_OFS_NUM_EXTENTS - VMFS_LVMINFO_OFFSET,
         lvm->extents[i]->vol_info.num_extents);
      m_pwrite(lvm->extents[i]->fd,buf,buf_len,
               lvm->extents[i]->vmfs_base + VMFS_LVMINFO_OFFSET);
   }
   return(0);
}

struct cmd {
   char *name;
   char *description;
   int (*fn)(vmfs_fs_t *fs,int argc,char *argv[]);
};

struct cmd cmd_array[] = {
   { "remove", "Remove an extent", cmd_remove },
   { NULL, }
};

static void show_usage(char *prog_name)
{
   int i;
   char *name = basename(prog_name);

   fprintf(stderr,"%s " VERSION "\n",name);
   fprintf(stderr,"Syntax: %s <device_name...> <command> <args...>\n\n",name);
   fprintf(stderr,"Available commands:\n");

   for(i=0;cmd_array[i].name;i++)
      fprintf(stderr,"  - %s : %s\n",cmd_array[i].name,cmd_array[i].description);

   fprintf(stderr,"\n");
}

static struct cmd *cmd_find(char *name)
{
   int i;

   for(i=0;cmd_array[i].name;i++)
      if (!strcmp(cmd_array[i].name,name))
         return(&cmd_array[i]);

   return NULL;
}

int main(int argc,char *argv[])
{
   vmfs_fs_t *fs;
   struct cmd *cmd = NULL;
   int arg, ret;
   vmfs_flags_t flags;

   if (argc < 3) {
      show_usage(argv[0]);
      return(0);
   }

   /* Scan arguments for a command */
   for (arg = 1; arg < argc; arg++) {
      if ((cmd = cmd_find(argv[arg])))
         break;
   }

   if (!cmd) {
      show_usage(argv[0]);
      return(0);
   }

   flags.packed = 0;

   flags.read_write = 1;

   argv[arg] = NULL;
   if (!(fs = vmfs_fs_open(&argv[1], flags))) {
      fprintf(stderr,"Unable to open filesystem\n");
      exit(EXIT_FAILURE);
   }

   ret = cmd->fn(fs,argc-arg-1,&argv[arg+1]);
   vmfs_fs_close(fs);
   return(ret);
}
