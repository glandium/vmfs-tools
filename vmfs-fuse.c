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
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vmfs.h"

static vmfs_fs_t *fs;

static int vmfs_fuse_getattr(const char *path, struct stat *stbuf)
{
   return(vmfs_file_lstat(fs, path, stbuf) ? -ENOENT : 0);
}

static int vmfs_fuse_readdir(const char *path, void *buf,
                             fuse_fill_dir_t filler, off_t offset,
                             struct fuse_file_info *fi)
{
   const vmfs_dirent_t *entry;
   vmfs_dir_t *d;
   struct stat st = {0, };

   d = vmfs_dir_open_at(fs->root_dir, path);

   if (!d)
      return(-ENOENT);

   while((entry = vmfs_dir_read(d))) {
      st.st_mode = vmfs_file_type2mode(entry->type);
      if (filler(buf, entry->name, &st, 0))
         break;
   }
   vmfs_dir_close(d);
   return(0);
}

static int vmfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
   vmfs_file_t *file = vmfs_file_open_at(fs->root_dir, path);
   if (!file)
      return(-ENOENT);

   fi->fh = (uint64_t)(unsigned long)file;
   return(0);
}

static int vmfs_fuse_read(const char *path, char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi)
{
   vmfs_file_t *file = (vmfs_file_t *)(unsigned long)fi->fh;
   return vmfs_file_pread(file, (u_char *)buf, size, offset);
}

static int vmfs_fuse_release(const char *path, struct fuse_file_info *fi)
{
   vmfs_file_close((vmfs_file_t *)(unsigned long)fi->fh);

   return(0);
}


const static struct fuse_operations vmfs_oper = {
   .getattr = vmfs_fuse_getattr,
   .readdir = vmfs_fuse_readdir,
   .open = vmfs_fuse_open,
   .read = vmfs_fuse_read,
   .release = vmfs_fuse_release,
};

struct vmfs_fuse_opts {
   vmfs_lvm_t *lvm;
   int mountpoint_set;
};

static int vmfs_fuse_opts_func(void *data, const char *arg, int key,
                struct fuse_args *outargs)
{
   struct vmfs_fuse_opts *opts = (struct vmfs_fuse_opts *) data;
   struct stat st;
   if (key == FUSE_OPT_KEY_NONOPT) {
      if (opts->mountpoint_set) {
         fprintf(stderr, "'%s' is not allowed here\n", arg);
         return -1;
      }
      if (stat(arg, &st)) {
         fprintf(stderr, "Error stat()ing '%s'\n", arg);
         return -1;
      }
      if (S_ISDIR(st.st_mode)) {
         opts->mountpoint_set = 1;
      } else if (S_ISREG(st.st_mode) || S_ISBLK(st.st_mode)) {
         if (vmfs_lvm_add_extent(opts->lvm,arg) == -1) {
            fprintf(stderr,"Unable to open device/file \"%s\".\n",arg);
            return -1;
         }
         return 0;
      }
   }
   return 1;
}


int main(int argc, char *argv[])
{
   struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
   struct vmfs_fuse_opts opts = { 0, };
   int err = -1;

   vmfs_host_init();

   if (!(opts.lvm = vmfs_lvm_create(0))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      goto cleanup;
   }

   if ((fuse_opt_parse(&args, &opts, NULL, &vmfs_fuse_opts_func) == -1) ||
       (fuse_opt_add_arg(&args, "-odefault_permissions"))) {
      goto cleanup;
   }

   if (!(fs = vmfs_fs_create(opts.lvm))) {
      fprintf(stderr,"Unable to open filesystem\n");
      goto cleanup;
   }

   if (vmfs_fs_open(fs) == -1) {
      fprintf(stderr,"Unable to open volume.\n");
      goto cleanup;
   }

   err = fuse_main(args.argc, args.argv, &vmfs_oper, fs);

cleanup:
   vmfs_fs_close(fs);
   fuse_opt_free_args(&args);

   return err ? 1 : 0;
}
