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
 * 0x00: In format version >= 2, following byte is the number of 32-bit words
 *       constituting the beginning of a 512B block, followed by that number
 *       of 32-bit words.
 *       In format version < 2, following 512B are a raw block.
 * 0x01: following chars are the number of blocks (512B) with zeroed data - 1
 *       in a variable-length encoding.
 * 0x7f: following 4 bytes is the little-endian encoded Adler-32 checksum.
 *
 */

#define FORMAT_VERSION 2

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif
#include <sys/stat.h>
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

   fprintf(stderr, "Syntax: %s [-x|-r] <image>\n",name);
}

static size_t do_reads(void *buf, size_t sz, size_t count)
{
   ssize_t hlen = 0, len;

   while (hlen < count * sz) {
      len = read(0, buf, count * sz - hlen);
      if ((len < 0) && (errno != EINTR))
         die("Read error\n");
      if (len == 0)
         break;
      hlen += len;
      buf += len;
   }
   if ((hlen > 0) && (hlen % sz))
      die("Short read\n");
   return(hlen);
}

static size_t do_read(void *buf, size_t count)
{
   return do_reads(buf, count, 1);
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

static const u_char const zero_blk[BLK_SIZE] = {0,};

#define ADLER32_MODULO 65521

static struct {
   uint32_t sum1, sum2;
} adler32 = { 1, 0 };

static void adler32_add(const u_char *buf, size_t blks)
{
   size_t i;
   if (buf == zero_blk) {
      i = blks;
      while (i >= 65536 / BLK_SIZE) {
         adler32.sum2 = (adler32.sum2 + 65536 * adler32.sum1) % ADLER32_MODULO;
         i -= 65536 / BLK_SIZE;
      }
      adler32.sum2 = (adler32.sum2 + i * BLK_SIZE * adler32.sum1) % ADLER32_MODULO;
   } else {
      uint32_t sum;
      while(blks--) {
         for (i = 0, sum = 0; i < BLK_SIZE / 16; i++) {
            #define adler32_step sum += (*buf++); adler32.sum2 += sum
            #define fourtimes(stuff) stuff; stuff; stuff; stuff
            fourtimes(fourtimes(adler32_step));
         }
         adler32.sum2 += adler32.sum1 * BLK_SIZE;
         adler32.sum1 += sum;

         adler32.sum1 %= ADLER32_MODULO;
         adler32.sum2 %= ADLER32_MODULO;
      }
   }
}

static uint32_t adler32_sum()
{
   return adler32.sum1 | (adler32.sum2 << 16);
}

static uint32_t do_read_number(void)
{
   u_char num;
   do_read(&num, 1);
   if (num & 0x80)
      return ((uint32_t) num & 0x7f) | (do_read_number() << 7);
   return (uint32_t) num;
}

static void skip_zero_blocks(size_t blks)
{
   off_t pos;
   if ((pos = lseek(1, BLK_SIZE * blks, SEEK_CUR)) == -1)
      die("Seek error\n");
   ftruncate(1, pos);
}

static void write_zero_blocks_(size_t blks)
{
   while (blks--)
      do_write(zero_blk, BLK_SIZE);
}

static void (*write_zero_blocks)(size_t blks) = write_zero_blocks_;

static void write_blocks(const u_char *buf, size_t blks)
{
   adler32_add(buf, blks);

   if (buf == zero_blk)
      write_zero_blocks(blks);
   else
      do_write(buf, blks * BLK_SIZE);
}

static void do_extract_(void (*write_blocks)(const u_char *, size_t))
{
   u_char buf[BLK_SIZE];
   u_char version;
   u_char desc;
   uint32_t num;

   /* Read file header */
   do_read(buf, 8);
   if (strncmp((char *)buf, "VMFSIMG", 7))
      die("extract: not a VMFS image\n");

   if ((version = buf[7]) > FORMAT_VERSION)
      die("extract: unsupported image format\n");

   while (do_read(&desc, 1)) {
      switch (desc) {
      case 0x00:
         if (version >= 2) {
            num = do_read_number() * 4;
            memset(&buf[num], 0, BLK_SIZE - num);
         } else
            num = BLK_SIZE;
         do_read(buf, num);
         write_blocks(buf, 1);
         break;
      case 0x01:
         num = do_read_number();
         write_blocks(zero_blk, num + 1);
         break;
      case 0x7f:
         do_read(buf, 4);
         num = read_le32(buf, 0);
         if (num != adler32_sum())
            die("extract: checksum mismatch\n");
         break;
      default:
         die("extract: corrupted image\n");
      }
   }
}

static void do_extract(void)
{
   do_extract_(write_blocks);
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

   for (i = 0; i < BLK_SIZE / 8; i++)
      if (((uint64_t *)buf)[i])
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

static void do_init_image(void)
{
   const u_char const buf[8] =
      { 'V', 'M', 'F', 'S', 'I', 'M', 'G', FORMAT_VERSION };
   do_write(buf, 8);
}

static void import_blocks(const u_char *buf, size_t blks)
{
   static enum block_type last = none, current;
   static uint32_t consecutive = 0;

   do {
      current = detect_block_type(buf);
      if ((last != none) && (current != last)) {
         end_consecutive_blocks(last, consecutive);
         consecutive = 0;
      }
      last = current;
      switch (current) {
      case zero:
         if (buf == zero_blk) {
            if (consecutive > (uint32_t) -blks)
               end_consecutive_blocks(zero, (uint32_t) -1);
            adler32_add(zero_blk, blks);
            consecutive += blks;
            return;
         }
         adler32_add(zero_blk, 1);
         consecutive++;
         break;
      case raw:
         do {
            int i;
            for (i = BLK_SIZE / 4; i;i--)
               if (((uint32_t *)buf)[i - 1])
                  break;
            do_write("\0", 1);
            do_write_number(i);
            do_write(buf, i * 4);
            adler32_add(buf, 1);
         } while(0);
         break;
      case none:
         do {
            uint32_t sum = adler32_sum();
            u_char b[5] = { 0x7f, sum & 0xff, (sum >> 8) & 0xff,
                            (sum >> 16) & 0xff, (sum >> 24) & 0xff };
            do_write(b, 5);
         } while(0);
         return;
      }
      buf += BLK_SIZE;
   } while (--blks);
}

static void do_import(void)
{
   u_char buf[BLK_SIZE * 16];
   size_t len;
#ifdef __linux__
   struct stat st;
   int blocksize = 0;
   off_t filesize = 0;

   if ((fstat(0, &st) == 0) && S_ISREG(st.st_mode)) {
      ioctl(0, FIGETBSZ, &blocksize);
      filesize = st.st_size;
   }
#endif

   do_init_image();

#ifdef __linux__
   if (blocksize) {
      int i, max = (int) ((filesize + blocksize - 1) / blocksize);
      for (i = 0; i < max; i++) {
         int block = i;
         if (ioctl(0, FIBMAP, &block) < 0)
            goto fallback;
         if (block) {
            lseek(0, (off_t) i * blocksize, SEEK_SET);
            len = do_reads(buf, BLK_SIZE, blocksize / BLK_SIZE);
            import_blocks(buf, len / BLK_SIZE);
         } else if (i < max - 1)
            import_blocks(zero_blk, blocksize / BLK_SIZE);
         else
            import_blocks(zero_blk, (filesize % blocksize) / BLK_SIZE);
      }
      import_blocks(NULL, 0);
      return;
   }
fallback:
#endif
   while ((len = do_reads(buf, BLK_SIZE, 16)))
      import_blocks(buf, len / BLK_SIZE);

   import_blocks(NULL, 0);
}

static void do_reimport(void)
{
   do_init_image();

   do_extract_(import_blocks);

   import_blocks(NULL, 0);
}

static void do_verify(void)
{
   do_extract_(adler32_add);
}

int main(int argc,char *argv[])
{
   char *arg = NULL;
   void (*func)(void) = do_import;
   struct stat st;

   if (argc > 1) {
      if (strcmp(argv[1],"-x") == 0) {
         func = do_extract;
         argc--;
      } else if (strcmp(argv[1],"-r") == 0) {
         func = do_reimport;
         argc--;
      } else if (strcmp(argv[1],"-v") == 0) {
         func = do_verify;
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

   if ((fstat(1, &st) == 0) && S_ISREG(st.st_mode) &&
       ((st.st_size == 0) || !(fcntl(1, F_GETFL) & O_APPEND)))
      write_zero_blocks = skip_zero_blocks;

   func();
   return(0);
}
