/*
 * Utility functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#include "utils.h"

/* Convert an UUID into a string (TODO: respect VMware format ?) */
char *m_uuid_to_str(uuid_t uuid,char *str)
{
   uuid_unparse(uuid, str);
   return str;
}

/* Convert a timestamp to a string */
char *m_ctime(time_t *ct,char *buf,size_t buf_len)
{
   struct tm ctm;

   localtime_r(ct,&ctm);

   snprintf(buf,buf_len,"%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d",
            ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday,
            ctm.tm_hour, ctm.tm_min, ctm.tm_sec);
   return buf;
}

/* Dump a structure in hexa and ascii */
void mem_dump(FILE *f_output,u_char *pkt,u_int len)
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
