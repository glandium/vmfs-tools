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
/*
 * Utility functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <uuid/uuid.h>

#include "utils.h"

/* Convert an UUID into a string */
char *m_uuid_to_str(const uuid_t uuid,char *str)
{
   uint32_t time_low;
   uint32_t time_mid;
   uint16_t clock_seq;
   time_low = read_le32(uuid,0);
   time_mid = read_le32(uuid,4);
   clock_seq = read_le16(uuid,8);
   sprintf(str,
      "%02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      time_low >> 24,
      time_low >> 16 & 0xff,
      time_low >> 8 & 0xff,
      time_low & 0xff,
      time_mid >> 24,
      time_mid >> 16 & 0xff,
      time_mid >> 8 & 0xff,
      time_mid & 0xff,
      clock_seq >> 8 & 0xff,
      clock_seq & 0xff,
      uuid[10],
      uuid[11],
      uuid[12],
      uuid[13],
      uuid[14],
      uuid[15]);
   return str;
}

/* Convert a timestamp to a string */
char *m_ctime(const time_t *ct,char *buf,size_t buf_len)
{
   struct tm ctm;

   localtime_r(ct,&ctm);

   snprintf(buf,buf_len,"%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
            ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday,
            ctm.tm_hour, ctm.tm_min, ctm.tm_sec);
   return buf;
}

struct fmode_info {
   u_int flag;
   char c;
   int pos;
};

static struct fmode_info fmode_flags[] = {
   { S_IFDIR, 'd', 0 },
   { S_IFLNK, 'l', 0 },
   { S_IRUSR, 'r', 1 },
   { S_IWUSR, 'w', 2 },
   { S_IXUSR, 'x', 3 },
   { S_IRGRP, 'r', 4 },
   { S_IWGRP, 'w', 5 },
   { S_IXGRP, 'x', 6 },
   { S_IROTH, 'r', 7 },
   { S_IWOTH, 'w', 8 },
   { S_IXOTH, 'x', 9 },
   { S_ISUID, 's', 3 },
   { S_ISVTX, 't', 9 },
   { 0, 0, -1, },
};

/* Convert a file mode to a string */
char *m_fmode_to_str(u_int mode,char *buf)
{
   struct fmode_info *fi;
   int i;

   for(i=0;i<10;i++) 
      buf[i] = '-';
   buf[10] = 0;

   for(i=0;fmode_flags[i].flag;i++) {
      fi = &fmode_flags[i];

      if ((mode & fi->flag) == fi->flag)
         buf[fi->pos] = fi->c;
   }

   return buf;
}

/* Dump a structure in hexa and ascii */
void mem_dump(FILE *f_output,const u_char *pkt,u_int len)
{
   u_int x,i = 0, tmp;

   while (i < len)
   {
      if ((len - i) > 16)
         x = 16;
      else x = len - i;

      fprintf(f_output,"%4.4x: ",i);

      for (tmp=0;tmp<x;tmp++)
         fprintf(f_output,"%2.2x ",pkt[i+tmp]);
      for (tmp=x;tmp<16;tmp++) fprintf(f_output,"   ");

      for (tmp=0;tmp<x;tmp++) {
         char c = pkt[i+tmp];

         if (((c >= 'A') && (c <= 'Z')) ||
             ((c >= 'a') && (c <= 'z')) ||
             ((c >= '0') && (c <= '9')))
            fprintf(f_output,"%c",c);
         else
            fputs(".",f_output);
      }

      i += x;
      fprintf(f_output,"\n");
   }

   fprintf(f_output,"\n");
   fflush(f_output);
}

/* Count the number of bits set in a byte */
int bit_count(u_char val)
{
   static int qb[16] = {
      0, 1, 1, 2, 1, 2, 2, 3,
      1, 2, 2, 3, 2, 3, 3, 4,
   };

   return(qb[val >> 4] + qb[val & 0x0F]);
}

/* Allocate a buffer with alignment compatible for direct I/O */
u_char *iobuffer_alloc(size_t len)
{
   size_t buf_len;
   void *buf;

   buf_len = ALIGN_NUM(len,M_DIO_BLK_SIZE);

   if (posix_memalign(&buf,M_DIO_BLK_SIZE,buf_len) != 0)
      return NULL;

   return buf;
}

/* Free a buffer previously allocated by iobuffer_alloc() */
void iobuffer_free(u_char *buf)
{
   free(buf);
}
