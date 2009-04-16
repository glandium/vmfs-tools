/*
 * VMFS prototype (based on http://code.google.com/p/vmfs/ from fluidOps)
 * C.Fillot, 2009/04/15
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

/* Max and min macro */
#define m_max(a,b) (((a) > (b)) ? (a) : (b))
#define m_min(a,b) (((a) < (b)) ? (a) : (b))

#define M_UUID_BUFLEN  128

typedef unsigned char m_u8_t;
typedef unsigned int  m_u32_t;
typedef unsigned long long m_u64_t;
typedef unsigned char m_uuid_t[16];

static inline m_u32_t read_le32(u_char *p,int offset)
{
   return((m_u32_t)p[offset] |
          ((m_u32_t)p[offset+1] << 8) |
          ((m_u32_t)p[offset+2] << 16) |
          ((m_u32_t)p[offset+3] << 24));
}

static inline m_u64_t read_le64(u_char *p,int offset)
{
   return((m_u64_t)read_le32(p,offset) + 
          ((m_u64_t)read_le32(p,offset+4) << 32));
}

/* VMFS types - forward declarations */
typedef struct vmfs_volinfo vmfs_volinfo_t;
typedef struct vmfs_fsinfo vmfs_fsinfo_t;
typedef struct vmfs_bitmap_header vmfs_bitmap_header_t;
typedef struct vmfs_bitmap_entry  vmfs_bitmap_entry_t;
typedef struct vmfs_file_info vmfs_file_info_t;
typedef struct vmfs_file_record vmfs_file_record_t;
typedef struct vmfs_blk_array vmfs_blk_array_t;
typedef struct vmfs_blk_list vmfs_blk_list_t;
typedef struct vmfs_file vmfs_file_t;
typedef struct vmfs_volume vmfs_volume_t;

/* Block IDs */
#define VMFS_BLK_TYPE(blk_id)  ((blk_id) & 0x07)

enum {
   VMFS_BLK_TYPE_INVALID = 0,
   VMFS_BLK_TYPE_FB,     /* Full Block */
   VMFS_BLK_TYPE_SB,     /* Sub-Block */
   VMFS_BLK_TYPE_PB,     /* Pointer Block */
   VMFS_BLK_TYPE_FD,     /* File Descriptor */
   VMFS_BLK_TYPE_MAX,
};

/* Full-Block */
#define VMFS_BLK_FB_NUMBER(blk_id)   ((blk_id) >> 6)

/* Sub-Block */
#define VMFS_BLK_SB_SUBGROUP(blk_id) (((blk_id) >> 28) & 0x0f)
#define VMFS_BLK_SB_NUMBER(blk_id)   (((blk_id) & 0xfffffff) >> 6)

/* Pointer-Block */
#define VMFS_BLK_PB_SUBGROUP(blk_id) (((blk_id) >> 28) & 0x0f)
#define VMFS_BLK_PB_NUMBER(blk_id)   (((blk_id) & 0xfffffff) >> 6)

/* File Descriptor */
#define VMFS_BLK_FD_SUBGROUP(blk_id) (((blk_id) >> 6)  & 0x7fff)
#define VMFS_BLK_FD_NUMBER(blk_id)   (((blk_id) >> 22) & 0x3ff)

/* VMFS meta-files */
#define VMFS_FBB_FILENAME  ".fbb.sf"
#define VMFS_FDC_FILENAME  ".fdc.sf"
#define VMFS_PBC_FILENAME  ".pbc.sf"
#define VMFS_SBC_FILENAME  ".sbc.sf"
#define VMFS_VH_FILENAME   ".vh.sf"

#define VMFS_FDC_BASE       0x1400000

/* === Volume Info === */
#define VMFS_VOLINFO_BASE   0x100000
#define VMFS_VOLINFO_MAGIC  0xc001d00d

#define VMFS_VOLINFO_OFS_MAGIC 0x0000
#define VMFS_VOLINFO_OFS_VER   0x0004
#define VMFS_VOLINFO_OFS_NAME  0x0012
#define VMFS_VOLINFO_OFS_UUID  0x0082 
#define VMFS_VOLINFO_OFS_SIZE  0x0200
#define VMFS_VOLINFO_OFS_BLKS  0x0208

struct vmfs_volinfo {
   m_u32_t magic;
   m_u32_t version;
   char name[28];
   m_uuid_t uuid;

   m_u64_t size;
   m_u64_t blocks;
};

/* === FS Info === */
#define VMFS_FSINFO_BASE   0x1200000
#define VMFS_FSINFO_MAGIC  0x2fabf15e

#define VMFS_FSINFO_OFS_MAGIC    0x0000
#define VMFS_FSINFO_OFS_VOLVER   0x0004
#define VMFS_FSINFO_OFS_VER      0x0008
#define VMFS_FSINFO_OFS_UUID     0x0009
#define VMFS_FSINFO_OFS_LABEL    0x001d
#define VMFS_FSINFO_OFS_BLKSIZE  0x00a1

struct vmfs_fsinfo {
   m_u32_t magic;
   m_u32_t vol_version;
   m_u32_t version;
   m_uuid_t uuid;
   char label[128];

   m_u64_t block_size;
   m_uuid_t vol_uuid;
};

/* === Bitmap header === */
struct vmfs_bitmap_header {
   m_u32_t items_per_bitmap_entry;
   m_u32_t bmp_entries_per_area;
   m_u32_t hdr_size;
   m_u32_t data_size;
   m_u32_t area_size;
   m_u32_t total_items;
   m_u32_t area_count;
};

/* === Bitmap entry === */
#define VMFS_BITMAP_ENTRY_SIZE  0x400

struct vmfs_bitmap_entry {
   m_u32_t magic;
   m_u32_t position;
   m_u32_t id;
   m_u32_t total;
   m_u32_t free;
   m_u32_t alloc;
   m_u8_t bitmap[0];
};

/* === File Meta Info === */
#define VMFS_FILE_INFO_SIZE  0x800

#define VMFS_FILEINFO_OFS_GRP_ID     0x0000
#define VMFS_FILEINFO_OFS_POS        0x0004
#define VMFS_FILEINFO_OFS_ID         0x0200
#define VMFS_FILEINFO_OFS_ID2        0x0204
#define VMFS_FILEINFO_OFS_TYPE       0x020c
#define VMFS_FILEINFO_OFS_SIZE       0x0214
#define VMFS_FILEINFO_OFS_TS1        0x022c
#define VMFS_FILEINFO_OFS_TS2        0x0230
#define VMFS_FILEINFO_OFS_TS3        0x0234
#define VMFS_FILEINFO_OFS_UID        0x0238
#define VMFS_FILEINFO_OFS_GID        0x023c
#define VMFS_FILEINFO_OFS_MODE       0x0240

#define VMFS_FILEINFO_OFS_BLK_ARRAY  0x0400
#define VMFS_FILEINFO_BLK_COUNT      0x100

struct vmfs_file_info {
   m_u32_t group_id;
   m_u32_t position;
   m_u32_t id,id2;
   m_u32_t type;
   m_u64_t size;
   m_u32_t ts1,ts2,ts3;
   m_u32_t uid,gid;
   m_u32_t mode;
};

/* === File Record === */
#define VMFS_FILE_RECORD_SIZE    0x8c

#define VMFS_FILEREC_OFS_TYPE    0x0000
#define VMFS_FILEREC_OFS_BLK_ID  0x0004
#define VMFS_FILEREC_OFS_REC_ID  0x0008
#define VMFS_FILEREC_OFS_NAME    0x000c

struct vmfs_file_record {
   m_u32_t type;
   m_u32_t block_id;
   m_u32_t record_id;
   char name[128];
};

/* === VMFS block list === */
#define VMFS_BLK_ARRAY_COUNT  64

struct vmfs_blk_array {
   m_u32_t blk[VMFS_BLK_ARRAY_COUNT];
   struct vmfs_blk_array *next;
};

struct vmfs_blk_list {
   m_u32_t total;
   m_u32_t avail;
   vmfs_blk_array_t *head,*tail;
};

/* === VMFS file abstraction === */
struct vmfs_file {
   vmfs_volume_t *vol;
   vmfs_blk_list_t blk_list;
   vmfs_file_info_t file_info;

   /* Current position in file */
   off_t pos;

   /* ... */
};

/* === VMFS mounted-volume === */
struct vmfs_volume {
   char *filename;
   FILE *fd;
   int debug_level;

   /* VMFS volume base */
   off_t vmfs_base;

   /* FDC base */
   off_t fdc_base;

   /* Volume and FS information */
   vmfs_volinfo_t vol_info;
   vmfs_fsinfo_t fs_info;

   /* Meta-files containing file system structures */
   vmfs_file_t *fbb,*fdc,*pbc,*sbc,*vh,*root_dir;

   /* Bitmap headers of meta-files */
   vmfs_bitmap_header_t fbb_bmh,fdc_bmh,pbc_bmh,sbc_bmh;
};

/* Forward declarations */
static inline m_u64_t vmfs_vol_get_blocksize(vmfs_volume_t *vol);
ssize_t vmfs_vol_read_data(vmfs_volume_t *vol,off_t pos,u_char *buf,size_t len);
ssize_t vmfs_vol_read(vmfs_volume_t *vol,m_u32_t blk,off_t offset,
                      u_char *buf,size_t len);
ssize_t vmfs_file_read(vmfs_file_t *f,u_char *buf,size_t len);
int vmfs_file_seek(vmfs_file_t *f,off_t pos,int whence);
static vmfs_file_t *
vmfs_file_open_rec(vmfs_volume_t *vol,vmfs_file_record_t *rec);

/* ======================================================================== */
/* Utility functions                                                        */
/* ======================================================================== */

char *m_uuid_to_str(m_uuid_t uuid,char *str)
{
   snprintf(str,M_UUID_BUFLEN,
            "%2.2x%2.2x%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x-"
            "%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
            uuid[0],uuid[1],uuid[2],uuid[3],
            uuid[4],uuid[5],uuid[6],uuid[7],
            uuid[8],uuid[9],uuid[10],uuid[11],
            uuid[12],uuid[13],uuid[14],uuid[15]);

   return str;
}

/* Ugly function that dumps a structure in hexa and ascii. */
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

/* ======================================================================== */
/* Marshalling                                                              */
/* ======================================================================== */

/* Read volume information */
int vmfs_volinfo_read(vmfs_volinfo_t *vol,FILE *fd)
{
   u_char buf[1024];

   if (fseeko(fd,VMFS_VOLINFO_BASE,SEEK_SET) != 0)
      return(-1);

   if (fread(buf,sizeof(buf),1,fd) != 1)
      return(-1);

   vol->magic   = read_le32(buf,VMFS_VOLINFO_OFS_MAGIC);
   vol->version = read_le32(buf,VMFS_VOLINFO_OFS_VER);
   vol->size    = read_le64(buf,VMFS_VOLINFO_OFS_SIZE);
   vol->blocks  = read_le64(buf,VMFS_VOLINFO_OFS_BLKS);

   memcpy(vol->name,buf+VMFS_VOLINFO_OFS_NAME,sizeof(vol->name));
   memcpy(vol->uuid,buf+VMFS_VOLINFO_OFS_UUID,sizeof(vol->uuid));

   if (vol->magic != VMFS_VOLINFO_MAGIC) {
      fprintf(stderr,"VMFS VolInfo: invalid magic number 0x%8.8x\n",vol->magic);
      return(-1);
   }

   return(0);
}

/* Show volume information */
void vmfs_volinfo_show(vmfs_volinfo_t *vol)
{
   char uuid_str[M_UUID_BUFLEN];

   printf("VMFS Volume Information:\n");
   printf("  - Version : %d\n",vol->version);
   printf("  - Name    : %s\n",vol->name);
   printf("  - UUID    : %s\n",m_uuid_to_str(vol->uuid,uuid_str));
   printf("  - Size    : %llu Gb\n",vol->size / (1024*1048576));
   printf("  - Blocks  : %llu\n",vol->blocks);


   printf("\n");
}

/* Read filesystem information */
int vmfs_fsinfo_read(vmfs_fsinfo_t *fsi,FILE *fd,off_t base)
{
   u_char buf[512];

   if (fseek(fd,base+VMFS_FSINFO_BASE,SEEK_SET) != 0)
      return(-1);

   if (fread(buf,sizeof(buf),1,fd) != 1)
      return(-1);

   fsi->magic       = read_le32(buf,VMFS_FSINFO_OFS_MAGIC);
   fsi->vol_version = read_le32(buf,VMFS_FSINFO_OFS_VOLVER);
   fsi->version     = buf[VMFS_FSINFO_OFS_VER];

   fsi->block_size  = read_le32(buf,VMFS_FSINFO_OFS_BLKSIZE);

   memcpy(fsi->uuid,buf+VMFS_FSINFO_OFS_UUID,sizeof(fsi->uuid));
   memcpy(fsi->label,buf+VMFS_FSINFO_OFS_LABEL,sizeof(fsi->label));

   if (fsi->magic != VMFS_FSINFO_MAGIC) {
      fprintf(stderr,"VMFS FSInfo: invalid magic number 0x%8.8x\n",fsi->magic);
      return(-1);
   }

   return(0);
}

/* Show FS information */
void vmfs_fsinfo_show(vmfs_fsinfo_t *fsi)
{  
   char uuid_str[M_UUID_BUFLEN];

   printf("VMFS FS Information:\n");

   printf("  - Vol. Version : %d\n",fsi->vol_version);
   printf("  - Version      : %d\n",fsi->version);
   printf("  - Label        : %s\n",fsi->label);
   printf("  - UUID         : %s\n",m_uuid_to_str(fsi->uuid,uuid_str));
   printf("  - Block size   : %llu (0x%llx)\n",
          fsi->block_size,fsi->block_size);

   printf("\n");
}

/* Read a bitmap header */
int vmfs_bmh_read(vmfs_bitmap_header_t *bmh,u_char *buf)
{
   bmh->items_per_bitmap_entry = read_le32(buf,0x0);
   bmh->bmp_entries_per_area   = read_le32(buf,0x4);
   bmh->hdr_size               = read_le32(buf,0x8);
   bmh->data_size              = read_le32(buf,0xc);
   bmh->area_size              = read_le32(buf,0x10);
   bmh->total_items            = read_le32(buf,0x14);
   bmh->area_count             = read_le32(buf,0x18);
   return(0);
}

/* Show bitmap information */
void vmfs_bmh_show(vmfs_bitmap_header_t *bmh)
{
   printf("  - Items per bitmap entry: %d (0x%x)\n",
          bmh->items_per_bitmap_entry,bmh->items_per_bitmap_entry);

   printf("  - Bitmap entries per area: %d (0x%x)\n",
          bmh->bmp_entries_per_area,bmh->bmp_entries_per_area);

   printf("  - Header size: %d (0x%x)\n",bmh->hdr_size,bmh->hdr_size);
   printf("  - Data size: %d (0x%x)\n",bmh->data_size,bmh->data_size);
   printf("  - Area size: %d (0x%x)\n",bmh->area_size,bmh->area_size);
   printf("  - Area count: %d (0x%x)\n",bmh->area_count,bmh->area_count);
   printf("  - Total items: %d (0x%x)\n",bmh->total_items,bmh->total_items);
}

/* Read a bitmap entry */
int vmfs_bme_read(vmfs_bitmap_entry_t *bme,u_char *buf,int copy_bitmap)
{
   bme->magic    = read_le32(buf,0x0);
   bme->position = read_le32(buf,0x4);
   bme->id       = read_le32(buf,0x200);
   bme->total    = read_le32(buf,0x204);
   bme->free     = read_le32(buf,0x208);
   bme->alloc    = read_le32(buf,0x20c);

   if (copy_bitmap) {
      memcpy(bme->bitmap,&buf[0x210],(bme->total+7)/8);
   }

   return(0);
}

/* Read a file meta info */
int vmfs_fmi_read(vmfs_file_info_t *fmi,u_char *buf)
{
   fmi->group_id = read_le32(buf,VMFS_FILEINFO_OFS_GRP_ID);
   fmi->position = read_le32(buf,VMFS_FILEINFO_OFS_POS);
   fmi->id       = read_le32(buf,VMFS_FILEINFO_OFS_ID);
   fmi->id2      = read_le32(buf,VMFS_FILEINFO_OFS_ID2);
   fmi->type     = read_le32(buf,VMFS_FILEINFO_OFS_TYPE);
   fmi->size     = read_le64(buf,VMFS_FILEINFO_OFS_SIZE);
   fmi->ts1      = read_le32(buf,VMFS_FILEINFO_OFS_TS1);
   fmi->ts2      = read_le32(buf,VMFS_FILEINFO_OFS_TS2);
   fmi->ts3      = read_le32(buf,VMFS_FILEINFO_OFS_TS3);
   fmi->uid      = read_le32(buf,VMFS_FILEINFO_OFS_UID);
   fmi->gid      = read_le32(buf,VMFS_FILEINFO_OFS_GID);
   fmi->mode     = read_le32(buf,VMFS_FILEINFO_OFS_MODE);

   return(0);
}

/* Read a file descriptor */
int vmfs_frec_read(vmfs_file_record_t *frec,u_char *buf)
{
   frec->type      = read_le32(buf,VMFS_FILEREC_OFS_TYPE);
   frec->block_id  = read_le32(buf,VMFS_FILEREC_OFS_BLK_ID);
   frec->record_id = read_le32(buf,VMFS_FILEREC_OFS_REC_ID);
   memcpy(frec->name,buf+VMFS_FILEREC_OFS_NAME,sizeof(frec->name));
   return(0);
}

/* ======================================================================== */
/* Bitmap management                                                        */
/* ======================================================================== */

/* Get address of a given area (pointing to bitmap array) */
static inline off_t 
vmfs_bitmap_get_area_addr(vmfs_bitmap_header_t *bmh,u_int area)
{
   return(bmh->hdr_size + (area * bmh->area_size));
}

/* Get data address of a given area (after the bitmap array) */
static inline off_t
vmfs_bitmap_get_area_data_addr(vmfs_bitmap_header_t *bmh,u_int area)
{
   off_t addr;

   addr  = vmfs_bitmap_get_area_addr(bmh,area);
   addr += bmh->bmp_entries_per_area * VMFS_BITMAP_ENTRY_SIZE;
   return(addr);
}

/* Get address of a block */
off_t vmfs_bitmap_get_block_addr(vmfs_bitmap_header_t *bmh,m_u32_t blk)
{
   m_u32_t items_per_area;
   off_t addr;
   u_int area;

   items_per_area = bmh->bmp_entries_per_area * bmh->items_per_bitmap_entry;
   area = blk / items_per_area;

   addr  = vmfs_bitmap_get_area_data_addr(bmh,area);
   addr += (blk % items_per_area) * bmh->data_size;

   return(addr);
}

/* Count the total number of allocated items in a bitmap area */
m_u32_t vmfs_bitmap_area_allocated_items(vmfs_file_t *f,
                                         vmfs_bitmap_header_t *bmh,
                                         u_int area)

{
   u_char buf[VMFS_BITMAP_ENTRY_SIZE];
   vmfs_bitmap_entry_t entry;
   m_u32_t count;
   int i;

   vmfs_file_seek(f,vmfs_bitmap_get_area_addr(bmh,area),SEEK_SET);

   for(i=0,count=0;i<bmh->bmp_entries_per_area;i++) {
      if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
         break;

      vmfs_bme_read(&entry,buf,0);
      count += entry.alloc;
   }

   return count;
}

/* Count the total number of allocated items in a bitmap */
m_u32_t vmfs_bitmap_allocated_items(vmfs_file_t *f,vmfs_bitmap_header_t *bmh)
{
   m_u32_t count;
   u_int i;
   
   for(i=0,count=0;i<bmh->area_count;i++)
      count += vmfs_bitmap_area_allocated_items(f,bmh,i);

   return(count);
}

/* ======================================================================== */
/* Block lists                                                              */
/* ======================================================================== */

void vmfs_blk_list_init(vmfs_blk_list_t *list)
{
   list->avail = list->total = 0;
   list->head = list->tail = NULL;
}

int vmfs_blk_list_add_block(vmfs_blk_list_t *list,m_u32_t blk_id)
{
   vmfs_blk_array_t *array;
   int pos;

   if (list->avail == 0) {
      /* no room available, create a new array */
      if (!(array = malloc(sizeof(*array))))
         return(-1);

      if (list->tail != NULL)
         list->tail->next = array;
      else
         list->head = array;
      
      list->tail = array;
      
      list->avail = VMFS_BLK_ARRAY_COUNT;
   }

   pos = VMFS_BLK_ARRAY_COUNT - list->avail;

   list->tail->blk[pos] = blk_id;
   list->total++;
   list->avail--;
   return(0);
}

int vmfs_blk_list_get_block(vmfs_blk_list_t *list,u_int pos,m_u32_t *blk_id)
{
   vmfs_blk_array_t *array;
   u_int cpos = 0;

   if (pos >= list->total)
      return(-1);

   for(array=list->head;array;array=array->next) {
      if ((pos >= cpos) && (pos < (cpos + VMFS_BLK_ARRAY_COUNT))) {
         *blk_id = array->blk[pos - cpos];
         return(0);
      }
      
      cpos += VMFS_BLK_ARRAY_COUNT;
   }

   return(-1);
}

/* ======================================================================== */
/* File abstraction                                                         */
/* ======================================================================== */

/* Create a file structure */
static vmfs_file_t *vmfs_file_create_struct(vmfs_volume_t *vol)
{
   vmfs_file_t *f;

   if (!(f = calloc(1,sizeof(*f))))
      return NULL;

   f->vol = vol;
   vmfs_blk_list_init(&f->blk_list);
   return f;
}

/* Get file size */
static inline m_u64_t vmfs_file_get_size(vmfs_file_t *f)
{
   return(f->file_info.size);
}

/* Set position */
int vmfs_file_seek(vmfs_file_t *f,off_t pos,int whence)
{
   switch(whence) {
      case SEEK_SET:
         f->pos = pos;
         break;
      case SEEK_CUR:
         f->pos += pos;
         break;
      case SEEK_END:
         f->pos -= pos;
         break;
   }
   
   /* Normalize */
   if (f->pos < 0)
      f->pos = 0;
   else {
      if (f->pos > f->file_info.size)
         f->pos = f->file_info.size;
   }

   return(0);
}

/* Read data from a file */
ssize_t vmfs_file_read(vmfs_file_t *f,u_char *buf,size_t len)
{
   vmfs_bitmap_header_t *sbc_bmh;
   vmfs_file_t *sbc;
   u_int blk_pos;
   m_u32_t blk_id,blk_type;
   m_u64_t blk_size,blk_len;
   m_u64_t file_size,offset;
   ssize_t res,rlen = 0;
   size_t clen,exp_len;

   blk_size = vmfs_vol_get_blocksize(f->vol);
   file_size = vmfs_file_get_size(f);

   sbc = f->vol->sbc;
   sbc_bmh = &f->vol->sbc_bmh;

   while(len > 0) {
      blk_pos = f->pos / blk_size;

      if (vmfs_blk_list_get_block(&f->blk_list,blk_pos,&blk_id) == -1)
         break;

#if 0
      if (f->vol->debug_level > 1)
         printf("vmfs_file_read: reading block 0x%8.8x\n",blk_id);
#endif

      blk_type = VMFS_BLK_TYPE(blk_id);

      switch(blk_type) {
         /* Full-Block */
         case VMFS_BLK_TYPE_FB:
            offset = f->pos % blk_size;
            blk_len = blk_size - offset;
            exp_len = m_min(blk_len,len);
            clen = m_min(exp_len,file_size - f->pos);

#if 0
            printf("vmfs_file_read: f->pos=0x%llx, offset=0x%8.8llx\n",
                   (m_u64_t)f->pos,offset);
#endif

            res = vmfs_vol_read(f->vol,VMFS_BLK_FB_NUMBER(blk_id),offset,
                                buf,clen);
            break;

         /* Sub-Block */
         case VMFS_BLK_TYPE_SB: {
            m_u32_t sbc_subgroup,sbc_number,sbc_blk;
            off_t sbc_addr;

            offset = f->pos % sbc_bmh->data_size;
            blk_len = sbc_bmh->data_size - offset;
            exp_len = m_min(blk_len,len);
            clen = m_min(exp_len,file_size - f->pos);

            sbc_subgroup = VMFS_BLK_SB_SUBGROUP(blk_id);
            sbc_number   = VMFS_BLK_SB_NUMBER(blk_id);

            sbc_blk = sbc_number * sbc_bmh->items_per_bitmap_entry;
            sbc_blk += sbc_subgroup;

            sbc_addr = vmfs_bitmap_get_block_addr(sbc_bmh,sbc_blk);
            sbc_addr += offset;

            vmfs_file_seek(sbc,sbc_addr,SEEK_SET);
            res = vmfs_file_read(sbc,buf,clen);

            break;
         }

         default:
            fprintf(stderr,"VMFS: unknown block type 0x%2.2x\n",blk_type);
            return(-1);
      }

      /* Move file position and keep track of bytes currently read */
      f->pos += res;
      rlen += res;

      /* Move buffer position */
      buf += res;
      len -= res;

      /* Incomplete read, stop now */
      if (res < exp_len)
         break;
   }

   return(rlen);
}

/* Get the offset corresponding to a file meta-info in FDC file */
static inline off_t 
vmfs_get_meta_info_offset(vmfs_volume_t *vol,m_u32_t blk_id)
{
   m_u32_t subgroup,number;
   off_t fmi_addr;
   m_u32_t fdc_blk;

   subgroup = VMFS_BLK_FD_SUBGROUP(blk_id);
   number   = VMFS_BLK_FD_NUMBER(blk_id);

   /* Compute the address of the file meta-info in the FDC file */
   fdc_blk = subgroup * vol->fdc_bmh.items_per_bitmap_entry;
   fmi_addr  = vmfs_bitmap_get_block_addr(&vol->fdc_bmh,fdc_blk);
   fmi_addr += number * vol->fdc_bmh.data_size;

   return(fmi_addr);
}

/* Get the meta-file info associated to a file record */
static int vmfs_get_meta_info(vmfs_volume_t *vol,vmfs_file_record_t *rec,
                              u_char *buf)
{
   m_u32_t blk_id = rec->block_id;
   off_t fmi_addr;
   ssize_t len;

   if (VMFS_BLK_TYPE(blk_id) != VMFS_BLK_TYPE_FD)
      return(-1);

   fmi_addr = vmfs_get_meta_info_offset(vol,blk_id);

   if (vmfs_file_seek(vol->fdc,fmi_addr,SEEK_SET) == -1)
      return(-1);
   
   len = vmfs_file_read(vol->fdc,buf,vol->fdc_bmh.data_size);
   return((len == vol->fdc_bmh.data_size) ? 0 : -1);
}

/* Search for an entry into a directory */
static int vmfs_file_searchdir(vmfs_file_t *dir_entry,char *name,
                               vmfs_file_record_t *rec)
{
   u_char buf[VMFS_FILE_RECORD_SIZE];
   int dir_count;
   ssize_t len;

   dir_count = vmfs_file_get_size(dir_entry) / VMFS_FILE_RECORD_SIZE;
   vmfs_file_seek(dir_entry,0,SEEK_SET);
   
   while(dir_count > 0) {
      len = vmfs_file_read(dir_entry,buf,sizeof(buf));

      if (len != VMFS_FILE_RECORD_SIZE)
         return(-1);

      vmfs_frec_read(rec,buf);

      if (!strcmp(rec->name,name))
         return(1);
   }

   return(0);
}

/* Resolve a path name to a file record */
static int vmfs_resolve_path(vmfs_volume_t *vol,char *name,
                             vmfs_file_record_t *rec)
{
   vmfs_file_t *cur_dir,*sub_dir;
   char *ptr,*sl;

   cur_dir = vol->root_dir;
   ptr = name;
   
   for(;;) {
      sl = strchr(ptr,'/');

      if (sl != NULL)
         *sl = 0;

      printf("vmfs_resolve_path: resolving '%s'\n",ptr);

      if (*ptr == 0) {
         ptr = sl + 1;
         continue;
      }
             
      if (vmfs_file_searchdir(cur_dir,ptr,rec) != 1)
         return(-1);
      
      printf("Entry found ! \n");

      /* last token */
      if (sl == NULL)
         return(1);

      if (!(sub_dir = vmfs_file_open_rec(vol,rec)))
         return(-1);

      printf("Opened subdirectory!\n");

#if 0 /* TODO */
      if (cur_dir != vol->root_dir)
         vmfs_file_close(cur_dir);
#endif
      cur_dir = sub_dir;
      ptr = sl + 1;
   }
}

/* Resolve pointer blocks */
static int vmfs_file_resolve_pb(vmfs_file_t *f,m_u32_t blk_id)
{
   u_char buf[4096];
   vmfs_bitmap_header_t *pbc_bmh;
   vmfs_file_t *pbc;
   m_u32_t pbc_blk,dblk;
   m_u32_t subgroup,number;
   size_t len;
   ssize_t res;
   off_t addr;
   int i;

   pbc = f->vol->pbc;
   pbc_bmh = &f->vol->pbc_bmh;

   subgroup = VMFS_BLK_PB_SUBGROUP(blk_id);
   number   = VMFS_BLK_PB_NUMBER(blk_id);

   /* Compute the address of the indirect pointers block in the PBC file */
   pbc_blk = (number * pbc_bmh->items_per_bitmap_entry) + subgroup;
   addr = vmfs_bitmap_get_block_addr(pbc_bmh,pbc_blk);
   len  = pbc_bmh->data_size;

   vmfs_file_seek(pbc,addr,SEEK_SET);

   while(len > 0) {
      res = vmfs_file_read(pbc,buf,sizeof(buf));

      if (res != sizeof(buf))
         return(-1);

      for(i=0;i<res/4;i++) {
         dblk = read_le32(buf,i*4);
         vmfs_blk_list_add_block(&f->blk_list,dblk);
      }

      len -= res;
   }
   
   return(0);
}

/* Bind meta-file info */
static int vmfs_file_bind_meta_info(vmfs_file_t *f,u_char *fmi_buf)
{
   m_u32_t blk_id,blk_type;
   int i;

   vmfs_fmi_read(&f->file_info,fmi_buf);
   vmfs_blk_list_init(&f->blk_list);

   for(i=0;i<VMFS_FILEINFO_BLK_COUNT;i++) {
      blk_id   = read_le32(fmi_buf,VMFS_FILEINFO_OFS_BLK_ARRAY+(i*4));
      blk_type = VMFS_BLK_TYPE(blk_id);

      if (!blk_id)
         break;

      switch(blk_type) {
         /* Full-Block/Sub-Block: simply add it to the list */
         case VMFS_BLK_TYPE_FB:
         case VMFS_BLK_TYPE_SB:
            vmfs_blk_list_add_block(&f->blk_list,blk_id);
            break;

         /* Pointer-block: resolve links */
         case VMFS_BLK_TYPE_PB:
            if (vmfs_file_resolve_pb(f,blk_id) == -1) {
               fprintf(stderr,"VMFS: unable to resolve blocks\n");
               return(-1);
            }
            break;

         default:
            fprintf(stderr,
                    "vmfs_file_bind_meta_info: "
                    "unexpected block type 0x%2.2x!\n",
                    blk_type);
            return(-1);
      }
   }

   return(0);
}

/* Open a file based on a file record */
static vmfs_file_t *
vmfs_file_open_rec(vmfs_volume_t *vol,vmfs_file_record_t *rec)
{
   u_char buf[VMFS_FILE_INFO_SIZE];
   vmfs_file_t *f;

   if (!(f = vmfs_file_create_struct(vol)))
      return NULL;
   
   /* Read the meta-info */
   if (vmfs_get_meta_info(vol,rec,buf) == -1) {
      fprintf(stderr,"VMFS: Unable to get meta-info\n");
      return NULL;
   }

   /* Bind the associated meta-info */
   if (vmfs_file_bind_meta_info(f,buf) == -1)
      return NULL;

   return f;
}

/* Dump a file */
int vmfs_file_dump(vmfs_file_t *f,off_t pos,size_t len,FILE *fd_out)
{
   u_char *buf;
   ssize_t res;
   size_t clen,buf_len;

   if (!len)
      len = vmfs_file_get_size(f);

   buf_len = 0x100000;

   if (!(buf = malloc(buf_len)))
      return(-1);

   vmfs_file_seek(f,pos,SEEK_SET);

   while(len > 0) {
      clen = m_min(len,buf_len);
      res = vmfs_file_read(f,buf,clen);

      if (res < 0) {
         fprintf(stderr,"vmfs_file_dump: problem reading input file.\n");
         return(-1);
      }

      if (fwrite(buf,1,res,fd_out) != res) {
         fprintf(stderr,"vmfs_file_dump: error writing output file.\n");
         return(-1);
      }

      if (res < clen)
         break;

      len -= res;
   }

   free(buf);
   return(0);
}

/* ======================================================================== */
/* Mounted volume management                                                */
/* ======================================================================== */

/* Get block size of a volume */
static inline m_u64_t vmfs_vol_get_blocksize(vmfs_volume_t *vol)
{
   return(vol->fs_info.block_size);
}

/* Read a data block from the physical volume */
ssize_t vmfs_vol_read_data(vmfs_volume_t *vol,off_t pos,u_char *buf,size_t len)
{
   if (fseeko(vol->fd,pos,SEEK_SET) != 0)
      return(-1);

   return(fread(buf,1,len,vol->fd));
}

/* Read a block */
ssize_t vmfs_vol_read(vmfs_volume_t *vol,m_u32_t blk,off_t offset,
                      u_char *buf,size_t len)
{
   off_t pos;

   pos  = (m_u64_t)blk * vmfs_vol_get_blocksize(vol);
   pos += vol->vmfs_base + 0x1000000;
   pos += offset;

   return(vmfs_vol_read_data(vol,pos,buf,len));
}

/* Create a volume structure */
vmfs_volume_t *vmfs_vol_create(char *filename,int debug_level)
{
   vmfs_volume_t *vol;

   if (!(vol = calloc(1,sizeof(*vol))))
      return NULL;

   if (!(vol->filename = strdup(filename)))
      goto err_filename;

   if (!(vol->fd = fopen(vol->filename,"r"))) {
      perror("fopen");
      goto err_open;
   }

   vol->debug_level = debug_level;
   return vol;

 err_open:
   free(vol->filename);
 err_filename:
   free(vol);
   return NULL;
}

/* Read the root directory given its meta-info */
static int vmfs_read_rootdir(vmfs_volume_t *vol,u_char *fmi_buf)
{
   if (!(vol->root_dir = vmfs_file_create_struct(vol)))
      return(-1);

   if (vmfs_file_bind_meta_info(vol->root_dir,fmi_buf) == -1) {
      fprintf(stderr,"VMFS: unable to bind meta info to root directory\n");
      return(-1);
   }
   
   return(0);
}

/* Read bitmap header */
static int vmfs_read_bitmap_header(vmfs_file_t *f,vmfs_bitmap_header_t *bmh)
{
   u_char buf[512];

   vmfs_file_seek(f,0,SEEK_SET);

   if (vmfs_file_read(f,buf,sizeof(buf)) != sizeof(buf))
      return(-1);

   return(vmfs_bmh_read(bmh,buf));
}

/* Open a meta-file */
static vmfs_file_t *vmfs_open_meta_file(vmfs_volume_t *vol,char *name,
                                        vmfs_bitmap_header_t *bmh)
{
   u_char buf[VMFS_FILE_INFO_SIZE];
   vmfs_file_record_t rec;
   vmfs_file_t *f;
   off_t fmi_addr;

   if (!(f = vmfs_file_create_struct(vol)))
      return NULL;

   /* Search the file name in root directory */
   if (vmfs_file_searchdir(vol->root_dir,name,&rec) != 1)
      return NULL;
   
   /* Read the meta-info */
   fmi_addr = vmfs_get_meta_info_offset(vol,rec.block_id);
   fmi_addr += vol->fdc_base;

   if (vmfs_vol_read_data(vol,fmi_addr,buf,sizeof(buf)) != sizeof(buf))
      return NULL;

   /* Bind the associated meta-info */
   if (vmfs_file_bind_meta_info(f,buf) == -1)
      return NULL;

   /* Read the bitmap header */
   if ((bmh != NULL) && (vmfs_read_bitmap_header(f,bmh) == -1))
      return NULL;
   
   return f;
}

/* Open all the VMFS meta files */
static int vmfs_open_all_meta_files(vmfs_volume_t *vol)
{
   vol->fbb = vmfs_open_meta_file(vol,VMFS_FBB_FILENAME,&vol->fbb_bmh);
   vol->fdc = vmfs_open_meta_file(vol,VMFS_FDC_FILENAME,&vol->fdc_bmh);
   vol->pbc = vmfs_open_meta_file(vol,VMFS_PBC_FILENAME,&vol->pbc_bmh);
   vol->sbc = vmfs_open_meta_file(vol,VMFS_SBC_FILENAME,&vol->sbc_bmh);
   vol->vh  = vmfs_open_meta_file(vol,VMFS_VH_FILENAME,NULL);

   return(0);
}

/* Dump volume bitmaps */
int vmfs_vol_dump_bitmaps(vmfs_volume_t *vol)
{
   printf("FBB bitmap:\n");
   vmfs_bmh_show(&vol->fbb_bmh);

   printf("\nFDC bitmap:\n");
   vmfs_bmh_show(&vol->fdc_bmh);

   printf("\nPBC bitmap:\n");
   vmfs_bmh_show(&vol->pbc_bmh);

   printf("\nSBC bitmap:\n");
   vmfs_bmh_show(&vol->sbc_bmh);

   return(0);
}

/* Read FDC base information */
static int vmfs_read_fdc_base(vmfs_volume_t *vol)
{
   u_char buf[VMFS_FILE_INFO_SIZE];
   vmfs_file_info_t fmi;
   off_t fmi_pos;
   m_u64_t len;

   /* Read the header */
   if (vmfs_vol_read_data(vol,vol->fdc_base,buf,sizeof(buf)) < sizeof(buf))
      return(-1);

   vmfs_bmh_read(&vol->fdc_bmh,buf);

   if (vol->debug_level > 0) {
      printf("FDC bitmap:\n");
      vmfs_bmh_show(&vol->fdc_bmh);
   }

   /* Read the File Meta Info */
   fmi_pos = vol->fdc_base + vmfs_bitmap_get_area_data_addr(&vol->fdc_bmh,0);

   if (fseeko(vol->fd,fmi_pos,SEEK_SET) == -1)
      return(-1);
   
   printf("File Meta Info at @0x%llx\n",(m_u64_t)fmi_pos);

   len = vol->fs_info.block_size - (fmi_pos - vol->fdc_base);
   printf("Length: 0x%8.8llx\n",len);

   /* Read the root directory meta-info */
   if (fread(buf,vol->fdc_bmh.data_size,1,vol->fd) != 1)
      return(-1);

   vmfs_fmi_read(&fmi,buf);
   vmfs_read_rootdir(vol,buf);

   /* Read the meta files */
   vmfs_open_all_meta_files(vol);

   /* Dump bitmap info */
   if (vol->debug_level > 0)
      vmfs_vol_dump_bitmaps(vol);

   return(0);
}

/* Open a VMFS volume */
int vmfs_vol_open(vmfs_volume_t *vol)
{
   vol->vmfs_base = VMFS_VOLINFO_BASE;

   /* Read volume information */
   if (vmfs_volinfo_read(&vol->vol_info,vol->fd) == -1) {
      fprintf(stderr,"VMFS: Unable to read volume information\n");
      return(-1);
   }

   if (vol->debug_level > 0)
      vmfs_volinfo_show(&vol->vol_info);

   /* Read FS info */
   if (vmfs_fsinfo_read(&vol->fs_info,vol->fd,vol->vmfs_base) == -1) {
      fprintf(stderr,"VMFS: Unable to read FS information\n");
      return(-1);
   }

   if (vol->debug_level > 0)
      vmfs_fsinfo_show(&vol->fs_info);

   /* Compute position of FDC base */
   vol->fdc_base = vol->vmfs_base + VMFS_FDC_BASE;

   if (vol->debug_level > 0)
      printf("FDC base = @0x%llx\n",(m_u64_t)vol->fdc_base);

   /* Read FDC base information */
   if (vmfs_read_fdc_base(vol) == -1) {
      fprintf(stderr,"VMFS: Unable to read FDC information\n");
      return(-1);
   }

   printf("VMFS: volume opened successfully\n");
   return(0);
}

int main(int argc,char *argv[])
{
   vmfs_volume_t *vol;

   vol = vmfs_vol_create(argv[1],2);
   vmfs_vol_open(vol);

#if 0
   {
      FILE *fd;
      fd = fopen("fdc_dump","w");
      vmfs_file_dump(vol->fdc,0,0,fd);
      fclose(fd);
   }
#endif

#if 1
   {
      vmfs_file_record_t rec;
      vmfs_file_t *f;
      char buffer[] = "Test1/Test1.vmx";
      FILE *fd;

      vmfs_resolve_path(vol,buffer,&rec);

      fd = fopen("test.vmx","w");
      f = vmfs_file_open_rec(vol,&rec);
      vmfs_file_dump(f,0,0,fd);
      fclose(fd);
   }
#endif

#if 1
   {
      m_u32_t count;
      
      count = vmfs_bitmap_allocated_items(vol->fbb,&vol->fbb_bmh);
      printf("FBB allocated items: %d (0x%x)\n",count,count);

      count = vmfs_bitmap_allocated_items(vol->fdc,&vol->fdc_bmh);
      printf("FDC allocated items: %d (0x%x)\n",count,count);

      count = vmfs_bitmap_allocated_items(vol->pbc,&vol->pbc_bmh);
      printf("PBC allocated items: %d (0x%x)\n",count,count);

      count = vmfs_bitmap_allocated_items(vol->sbc,&vol->sbc_bmh);
      printf("SBC allocated items: %d (0x%x)\n",count,count);
   }
#endif

   return(0);
}
