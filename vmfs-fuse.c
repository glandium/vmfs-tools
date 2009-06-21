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

#include <fuse_lowlevel.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vmfs.h"

static vmfs_fs_t *fs;

static inline uint32_t ino2blkid(fuse_ino_t ino)
{
   if (ino == FUSE_ROOT_ID)
      return(VMFS_BLK_FD_BUILD(0,0));
   return((uint32_t)ino);
}

static inline fuse_ino_t blkid2ino(uint32_t blk_id)
{
   if (blk_id == VMFS_BLK_FD_BUILD(0,0))
      return(FUSE_ROOT_ID);
   return((fuse_ino_t)blk_id);
}

static void vmfs_fuse_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
   vmfs_fs_t *fs = (vmfs_fs_t *) fuse_req_userdata(req);
   struct stat stbuf;

   if (!vmfs_inode_stat_from_blkid(fs, ino2blkid(ino), &stbuf)) {
      stbuf.st_ino = ino;
      fuse_reply_attr(req, &stbuf, 1.0);
   } else
      fuse_reply_err(req, ENOENT);
}

static void vmfs_fuse_opendir(fuse_req_t req, fuse_ino_t ino,
                              struct fuse_file_info *fi)
{
   vmfs_fs_t *fs = (vmfs_fs_t *) fuse_req_userdata(req);

   fi->fh = (uint64_t)(unsigned long)
            vmfs_dir_open_from_blkid(fs, ino2blkid(ino));
   if (fi->fh)
      fuse_reply_open(req, fi);
   else
      fuse_reply_err(req, ENOTDIR);
}

static void vmfs_fuse_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                              off_t off, struct fuse_file_info *fi)
{
   char buf[size];
   const vmfs_dirent_t *entry;
   struct stat st = {0, };
   size_t sz;

   if (!fi->fh) {
      fuse_reply_err(req, EBADF);
      return;
   }
   if ((entry = vmfs_dir_read((vmfs_dir_t *)(unsigned long)fi->fh))) {
      st.st_mode = vmfs_file_type2mode(entry->type);
      st.st_ino = blkid2ino(entry->block_id);
      sz = fuse_add_direntry(req, buf, size, entry->name, &st, off + 1);
      fuse_reply_buf(req, buf, sz);
   } else
      fuse_reply_buf(req, NULL, 0);
}

static void vmfs_fuse_releasedir(fuse_req_t req, fuse_ino_t ino,
                                 struct fuse_file_info *fi)
{
   if (!fi->fh) {
      fuse_reply_err(req, EBADF);
      return;
   }
   vmfs_dir_close((vmfs_dir_t *)(unsigned long)fi->fh);
   fuse_reply_err(req, 0);
}

#if 0
static int vmfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
   vmfs_file_t *file;
   vmfs_dir_t *root_dir;

   if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0))))
      return(-ENOMEM);

   file = vmfs_file_open_at(root_dir, path);
   vmfs_dir_close(root_dir);

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
#endif

const static struct fuse_lowlevel_ops vmfs_oper = {
   .getattr = vmfs_fuse_getattr,
   .opendir = vmfs_fuse_opendir,
   .readdir = vmfs_fuse_readdir,
   .releasedir = vmfs_fuse_releasedir,
#if 0
   .open = vmfs_fuse_open,
   .read = vmfs_fuse_read,
   .release = vmfs_fuse_release,
#endif
};

struct vmfs_fuse_opts {
   vmfs_lvm_t *lvm;
   char *mountpoint;
   int foreground;
};

static const struct fuse_opt vmfs_fuse_args[] = {
  { "-d", offsetof(struct vmfs_fuse_opts, foreground), 1 },
  { "-f", offsetof(struct vmfs_fuse_opts, foreground), 1 },
  FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
};

static int vmfs_fuse_opts_func(void *data, const char *arg, int key,
                struct fuse_args *outargs)
{
   struct vmfs_fuse_opts *opts = (struct vmfs_fuse_opts *) data;
   struct stat st;
   if (key == FUSE_OPT_KEY_NONOPT) {
      if (opts->mountpoint) {
         fprintf(stderr, "'%s' is not allowed here\n", arg);
         return -1;
      }
      if (stat(arg, &st)) {
         fprintf(stderr, "Error stat()ing '%s'\n", arg);
         return -1;
      }
      if (S_ISDIR(st.st_mode)) {
         opts->mountpoint = strdup(arg);
      } else if (S_ISREG(st.st_mode) || S_ISBLK(st.st_mode)) {
         if (vmfs_lvm_add_extent(opts->lvm,arg) == -1) {
            fprintf(stderr,"Unable to open device/file \"%s\".\n",arg);
            return -1;
         }
      }
      return 0;
   }
   return 1;
}

int main(int argc, char *argv[])
{
   struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
   struct vmfs_fuse_opts opts = { 0, };
   struct fuse_chan *chan;
   int err = -1;

   vmfs_host_init();

   if (!(opts.lvm = vmfs_lvm_create(0))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      goto cleanup;
   }

   if ((fuse_opt_parse(&args, &opts, vmfs_fuse_args,
                       &vmfs_fuse_opts_func) == -1) ||
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

   if ((chan = fuse_mount(opts.mountpoint, &args)) != NULL) {
   struct fuse_session *session;
      session = fuse_lowlevel_new(&args, &vmfs_oper,
                                  sizeof(vmfs_oper), fs);
      if (session != NULL) {
         fuse_daemonize(opts.foreground);
         if (fuse_set_signal_handlers(session) != -1) {
            fuse_session_add_chan(session, chan);
            err = fuse_session_loop_mt(session);
            fuse_remove_signal_handlers(session);
            fuse_session_remove_chan(chan);
         }
         fuse_session_destroy(session);
      }
      fuse_unmount(opts.mountpoint, chan);
   }

cleanup:
   vmfs_fs_close(fs);
   fuse_opt_free_args(&args);

   return err ? 1 : 0;
}
