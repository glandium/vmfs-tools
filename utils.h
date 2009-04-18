#ifndef UTILS_H
#define UTILS_H

#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <uuid/uuid.h>

/* Max and min macro */
#define m_max(a,b) (((a) > (b)) ? (a) : (b))
#define m_min(a,b) (((a) < (b)) ? (a) : (b))

#define M_UUID_BUFLEN  128

typedef unsigned char m_u8_t;
typedef unsigned short m_u16_t;
typedef unsigned int m_u32_t;
typedef unsigned long long m_u64_t;

/* Read a 16-bit word in little endian format */
static inline m_u16_t read_le16(u_char *p,int offset)
{
   return((m_u16_t)p[offset] | ((m_u16_t)p[offset+1] << 8));
}

/* Read a 32-bit word in little endian format */
static inline m_u32_t read_le32(u_char *p,int offset)
{
   return((m_u32_t)p[offset] |
          ((m_u32_t)p[offset+1] << 8) |
          ((m_u32_t)p[offset+2] << 16) |
          ((m_u32_t)p[offset+3] << 24));
}

/* Read a 64-bit word in little endian format */
static inline m_u64_t read_le64(u_char *p,int offset)
{
   return((m_u64_t)read_le32(p,offset) + 
          ((m_u64_t)read_le32(p,offset+4) << 32));
}

/* Convert an UUID into a string */
char *m_uuid_to_str(uuid_t uuid,char *str);

/* Dump a structure in hexa and ascii */
void mem_dump(FILE *f_output,u_char *pkt,u_int len);

/* Count the number of bits set in a byte */
int bit_count(u_char val);

#endif
