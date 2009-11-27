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
#include "vmfs.h"

static int cmd_remove(vmfs_fs_t *fs,int argc,char *argv[])
{
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
