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

/*
 * An image file consists of a header followed by a set of sequences, each
 * of which starts with a byte describing the sequence.
 *
 * The header is "VMFSIMG" followed by a byte containing the format version.
 * Backwards compatibility is to be preserved.
 *
 * The following sequence descriptor codes are used:
 * 0x00: following 512B are a raw block.
 * 0x01: following chars are the number of blocks (512B) with zeroed data - 1
 *       in a variable-length encoding.
 *
 */

#define FORMAT_VERSION 0

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

static void do_write(const void *buf, size_t count)
{
   ssize_t hlen = 0, len;

   while (hlen < count) {
      len = write(1, buf, count-hlen);
      if ((len < 0) && (errno != EINTR))
         die("Write error\n");
      if (len == 0)
         break;
      hlen += len;
      buf += len;
   }
   if (hlen != count)
      die("Short write\n");
}

#define BLK_SIZE 512

static uint32_t do_read_number(void)
{
   u_char num;
   do_read(&num, 1);
   if (num & 0x80)
      return ((uint32_t) num & 0x7f) | (do_read_number() << 7);
   return (uint32_t) num;
}

static void do_extract(void)
{
   u_char buf[BLK_SIZE];
   u_char zero_blk[BLK_SIZE] = {0,};
   u_char desc;
   uint32_t num;

   /* Read file header */
   do_read(buf, 8);
   if (strncmp((char *)buf, "VMFSIMG", 7))
      die("extract: not a VMFS image\n");

   if (buf[7] > FORMAT_VERSION)
      die("extract: unsupported image format\n");

   while (do_read(&desc, 1)) {
      switch (desc) {
      case 0x00:
         do_read(buf, BLK_SIZE);
         do_write(buf, BLK_SIZE);
         break;
      case 0x01:
         num = do_read_number();
         do {
            do_write(zero_blk, BLK_SIZE);
         } while (num--);
         break;
      default:
         die("extract: corrupted image\n");
      }
   }
}

static void do_write_number(uint32_t num)
{
   u_char buf[5], *b = buf;
   size_t sz;
   do {
      *b = (u_char) (num & 0x7f);
   } while ((num >>= 7) && (*(b++) |= 0x80));
   sz = b - buf + 1;
   do_write(buf, sz);
}

enum block_type {
   none,
   zero,
   raw,
};

static enum block_type detect_block_type(const u_char *buf)
{
   int i;

   if (buf == NULL)
      return none;

   for (i = 0; i < BLK_SIZE; i++)
      if (buf[i])
         return raw;

   return zero;
}

static void end_consecutive_blocks(enum block_type type, uint32_t blks)
{
   if (type == zero) {
      do_write("\1", 1);
      do_write_number(blks - 1);
   }
}

static void do_import(void)
{
   u_char buf[BLK_SIZE];
   enum block_type last = none, current;
   uint32_t consecutive = 0;

   /* Write the file header */
   do_write("VMFSIMG", 7);
   buf[0] = FORMAT_VERSION;
   do_write(buf, 1);

   while (do_read(buf, BLK_SIZE)) {
      current = detect_block_type(buf);
      if ((last != none) && (current != last)) {
         end_consecutive_blocks(last, consecutive);
         consecutive = 0;
      }
      last = current;
      switch (current) {
      case zero:
         consecutive++;
         break;
      case raw:
         do_write("\0", 1);
         do_write(buf, BLK_SIZE);
         break;
      case none:
         return;
      }
   }
   end_consecutive_blocks(last, consecutive);
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
