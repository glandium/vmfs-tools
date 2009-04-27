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
   vmfs_dirent_t **dlist;
   struct stat st = {0, };
   int i,num;
   num = vmfs_dirent_readdir(fs, path, &dlist);
   for (i = 0; i < num; i++) {
      st.st_mode = vmfs_file_type2mode(dlist[i]->type);
      if (filler(buf, dlist[i]->name, &st, 0))
         break;
   }
   vmfs_dirent_free_dlist(num, &dlist);
   return(0);
}

static int vmfs_fuse_open(const char *path, struct fuse_file_info *fi)
{
   vmfs_file_t *file = vmfs_file_open(fs, path);
   if (!file)
      return -ENOENT;

   fi->fh = (uint64_t)(unsigned long)file;
   return(0);
}

static int vmfs_fuse_read(const char *path, char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi)
{
   vmfs_file_t *file = (vmfs_file_t *)(unsigned long)fi->fh;
   vmfs_file_seek(file, offset, SEEK_SET); /* This is not thread-safe */
   return vmfs_file_read(file, (u_char *)buf, size);
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
   char *dev;
};

static int vmfs_fuse_opts_func(void *data, const char *arg, int key,
                struct fuse_args *outargs)
{
   struct vmfs_fuse_opts *opts = (struct vmfs_fuse_opts *) data;
   if (key == FUSE_OPT_KEY_NONOPT) {
      if (opts->dev == NULL) {
         opts->dev = strdup(arg);
         return 0;
      }
   }
   return 1;
}


int main(int argc, char *argv[])
{
   struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
   struct vmfs_fuse_opts opts = { 0, };
   vmfs_lvm_t *lvm;
   int err = -1;

   if ((fuse_opt_parse(&args, &opts, NULL, &vmfs_fuse_opts_func) == -1) ||
       (opts.dev == NULL) ||
       (fuse_opt_add_arg(&args, "-odefault_permissions"))) {
      goto cleanup;
   }

   if (!(lvm = vmfs_lvm_create(0))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      goto cleanup;
   }

   if (vmfs_lvm_add_extent(lvm,opts.dev) == -1) {
      fprintf(stderr,"Unable to open device/file \"%s\".\n",opts.dev);
      goto cleanup;
   }

   if (!(fs = vmfs_fs_create(lvm))) {
      fprintf(stderr,"Unable to open filesystem\n");
      goto cleanup;
   }

   if (vmfs_fs_open(fs) == -1) {
      fprintf(stderr,"Unable to open volume.\n");
      goto cleanup;
   }

   err = fuse_main(args.argc, args.argv, &vmfs_oper, fs);

cleanup:
   fuse_opt_free_args(&args);
   free(opts.dev);

   return err ? 1 : 0;
}
