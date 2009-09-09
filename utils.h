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
#include <uuid.h>
#include <inttypes.h>

/* Max and min macro */
#define m_max(a,b) (((a) > (b)) ? (a) : (b))
#define m_min(a,b) (((a) < (b)) ? (a) : (b))

#define M_UUID_BUFLEN  36

#if defined(__amd64__) || defined(__i386__)
#define LE_AND_NO_ALIGN 1
#endif

/* Read a 16-bit word in little endian format */
static inline uint16_t read_le16(const u_char *p,int offset)
{
#ifdef LE_AND_NO_ALIGN
   return(*((uint16_t *)&p[offset]));
#else
   return((uint16_t)p[offset] | ((uint16_t)p[offset+1] << 8));
#endif
}

/* Write a 16-bit word in little endian format */
static inline void write_le16(u_char *p,int offset,uint16_t val)
{
#ifdef LE_AND_NO_ALIGN
   *(uint16_t *)&p[offset] = val;
#else
   p[offset]   = val & 0xFF;
   p[offset+1] = val >> 8;
#endif
}

/* Read a 32-bit word in little endian format */
static inline uint32_t read_le32(const u_char *p,int offset)
{
#ifdef LE_AND_NO_ALIGN
   return(*((uint32_t *)&p[offset]));
#else
   return((uint32_t)p[offset] |
          ((uint32_t)p[offset+1] << 8) |
          ((uint32_t)p[offset+2] << 16) |
          ((uint32_t)p[offset+3] << 24));
#endif
}

/* Write a 32-bit word in little endian format */
static inline void write_le32(u_char *p,int offset,uint32_t val)
{
#ifdef LE_AND_NO_ALIGN
   *(uint32_t *)&p[offset] = val;
#else
   p[offset]   = val & 0xFF;
   p[offset+1] = val >> 8;
   p[offset+2] = val >> 16;
   p[offset+3] = val >> 24;
#endif
}

/* Read a 64-bit word in little endian format */
static inline uint64_t read_le64(const u_char *p,int offset)
{
#ifdef LE_AND_NO_ALIGN
   return(*((uint64_t *)&p[offset]));
#else
   return((uint64_t)read_le32(p,offset) +
          ((uint64_t)read_le32(p,offset+4) << 32));
#endif
}

/* Write a 64-bit word in little endian format */
static inline void write_le64(u_char *p,int offset,uint64_t val)
{
#ifdef LE_AND_NO_ALIGN
   *(uint64_t *)&p[offset] = val;
#else
   write_le32(p,offset,val);
   write_le32(p,offset+4,val);
#endif
}

#define cpu_to_lexx(bits) \
   static inline uint##bits##_t cpu_to_le##bits(uint##bits##_t i) \
   { \
      u_char *p = (u_char *)&i; \
      return read_le##bits(p, 0); \
   }

cpu_to_lexx(16)
cpu_to_lexx(32)
cpu_to_lexx(64)

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

#define M_SECTOR_SIZE  512
#define M_BLK_SIZE     4096

/* 
 * Block size/alignment required for direct I/O : 
 *    4k bytes on Linux 2.4,
 *    512 bytes on Linux 2.6
 */
#define M_DIO_BLK_SIZE  4096

#define ALIGN_CHECK(val, mult)  (((val) & ((mult) - 1)) == 0)
#define ALIGN_NUM(val, mult) (((val) + ((mult) - 1)) & ~(((mult) - 1)))
#define ALIGN_PTR(ptr, mult) (void *)ALIGN_NUM((uintptr_t)(ptr), mult)

#define DECL_ALIGNED_BUFFER(name, size) \
   u_char __##name[(size) + M_SECTOR_SIZE]; \
   u_char *name = (u_char *)ALIGN_PTR(__##name,M_SECTOR_SIZE); \
   size_t name##_len = (size)

#define DECL_ALIGNED_BUFFER_WOL(name, size) \
   u_char __##name[(size) + M_SECTOR_SIZE]; \
   u_char *name = (u_char *)ALIGN_PTR(__##name,M_SECTOR_SIZE); \

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

/* Allocate a buffer with alignment compatible for direct I/O */
u_char *iobuffer_alloc(size_t len);

/* Free a buffer previously allocated by iobuffer_alloc() */
void iobuffer_free(u_char *buf);

/* Read from file descriptor at a given offset */
ssize_t m_pread(int fd,void *buf,size_t count,off_t offset);

/* Write to a file descriptor at a given offset */
ssize_t m_pwrite(int fd,const void *buf,size_t count,off_t offset);

/* Returns directory name */
char *m_dirname(const char *path);

/* Returns base name */
char *m_basename(const char *path);

#ifdef NO_STRNDUP
static inline char *strndup(const char *s, size_t n) {
   char *result;
   n = strnlen(s, n);
   result = malloc(n + 1);
   if (!result)
      return NULL;
   memcpy(result, s, n);
   result[n + 1] = 0;
}
#endif

#endif
