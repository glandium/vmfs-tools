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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "vmfs.h"

static void die(char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vfprintf(stderr, fmt, ap);
   va_end(ap);
   exit(1);
}

static void show_usage(char *prog_name)
{
   char *name = basename(prog_name);

   fprintf(stderr, "Syntax: %s [-x] <image>\n",name);
}

static size_t do_read(void *buf, size_t count)
{
   ssize_t hlen = 0, len;

   while (hlen < count) {
      len = read(0, buf, count-hlen);
      if ((len < 0) && (errno != EINTR))
         die("Read error\n");
      if (len == 0)
         break;
      hlen += len;
      buf += len;
   }
   if ((hlen > 0) && (hlen != count))
      die("Short read\n");
   return(hlen);
}

#define BUF_SIZE 4096

static void do_extract(void)
{
   u_char buf[BUF_SIZE];

   while (do_read(buf, BUF_SIZE))
      write(1, buf, BUF_SIZE);
}

static void do_import(void)
{
   u_char buf[BUF_SIZE];

   while (do_read(buf, BUF_SIZE))
      write(1, buf, BUF_SIZE);
}

int main(int argc,char *argv[])
{
   char *arg = NULL;
   void (*func)(void) = do_import;

   if (argc > 1) {
      if (strcmp(argv[1],"-x") == 0) {
         func = do_extract;
         argc--;
      }
      if (argc == 2)
         arg = argv[(func == do_import) ? 1 : 2];
   }
   if (argc > 2) {
      show_usage(argv[0]);
      return(0);
   }

   if (arg) {
      int fd = open(arg, O_RDONLY);
      if (fd == -1) {
         fprintf(stderr, "Error opening %s: %s\n", arg, strerror(errno));
         return(1);
      }
      dup2(fd,0);
      close(fd);
   }

   func();
   return(0);
}
