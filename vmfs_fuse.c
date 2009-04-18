#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <string.h>

const static struct fuse_operations vmfs_oper = {
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
   int err = -1;

   if ((fuse_opt_parse(&args, &opts, NULL, &vmfs_fuse_opts_func) == -1) ||
       (opts.dev == NULL) ||
       (fuse_opt_add_arg(&args, "-odefault_permissions"))) {
      goto cleanup;
   }

   err = fuse_main(args.argc, args.argv, &vmfs_oper, NULL);

cleanup:
   fuse_opt_free_args(&args);
   free(opts.dev);

   return err ? 1 : 0;
}
