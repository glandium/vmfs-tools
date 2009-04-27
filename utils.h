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
#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>

/* Max and min macro */
#define m_max(a,b) (((a) > (b)) ? (a) : (b))
#define m_min(a,b) (((a) < (b)) ? (a) : (b))

#define M_UUID_BUFLEN  36

typedef unsigned char m_u8_t;
typedef unsigned short m_u16_t;
typedef unsigned int m_u32_t;
typedef unsigned long long m_u64_t;

#if defined(__amd64__) || defined(__i386__)
#define LE_AND_NO_ALIGN 1
#endif

/* Read a 16-bit word in little endian format */
static inline m_u16_t read_le16(const u_char *p,int offset)
{
#ifdef LE_AND_NO_ALIGN
   return(*((m_u16_t *)&p[offset]));
#else
   return((m_u16_t)p[offset] | ((m_u16_t)p[offset+1] << 8));
#endif
}

/* Write a 16-bit word in little endian format */
static inline void write_le16(u_char *p,int offset,m_u16_t val)
{
#ifdef LE_AND_NO_ALIGN
   *(m_u16_t *)&p[offset] = val;
#else
   p[offset]   = val & 0xFF;
   p[offset+1] = val >> 8;
#endif
}

/* Read a 32-bit word in little endian format */
static inline m_u32_t read_le32(const u_char *p,int offset)
{
#ifdef LE_AND_NO_ALIGN
   return(*((m_u32_t *)&p[offset]));
#else
   return((m_u32_t)p[offset] |
          ((m_u32_t)p[offset+1] << 8) |
          ((m_u32_t)p[offset+2] << 16) |
          ((m_u32_t)p[offset+3] << 24));
#endif
}

/* Write a 32-bit word in little endian format */
static inline void write_le32(u_char *p,int offset,m_u32_t val)
{
#ifdef LE_AND_NO_ALIGN
   *(m_u32_t *)&p[offset] = val;
#else
   p[offset]   = val & 0xFF;
   p[offset+1] = val >> 8;
   p[offset+2] = val >> 16;
   p[offset+3] = val >> 24;
#endif
}

/* Read a 64-bit word in little endian format */
static inline m_u64_t read_le64(const u_char *p,int offset)
{
#ifdef LE_AND_NO_ALIGN
   return(*((m_u64_t *)&p[offset]));
#else
   return((m_u64_t)read_le32(p,offset) + 
          ((m_u64_t)read_le32(p,offset+4) << 32));
#endif
}

/* Write a 64-bit word in little endian format */
static inline void write_le64(u_char *p,int offset,m_u64_t val)
{
#ifdef LE_AND_NO_ALIGN
   *(m_u64_t *)&p[offset] = val;
#else
   write_le32(p,offset,val);
   write_le32(p,offset+4,val);
#endif
}

/* Read an UUID at a given offset in a buffer */
static inline void read_uuid(const u_char *buf,int offset,uuid_t *uuid)
{
   memcpy(uuid,buf+offset,sizeof(uuid_t));
}

/* Write an UUID to a given offset in a buffer */
static inline void write_uuid(u_char *buf,int offset,const uuid_t *uuid)
{
   memcpy(buf+offset,uuid,sizeof(uuid_t));
}

/* Convert an UUID into a string */
char *m_uuid_to_str(const uuid_t uuid,char *str);

/* Convert a timestamp to a string */
char *m_ctime(const time_t *ct,char *buf,size_t buf_len);

/* Convert a file mode to a string */
char *m_fmode_to_str(u_int mode,char *buf);

/* Dump a structure in hexa and ascii */
void mem_dump(FILE *f_output,const u_char *pkt,u_int len);

/* Count the number of bits set in a byte */
int bit_count(u_char val);

#endif
