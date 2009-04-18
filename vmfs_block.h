#ifndef VMFS_BLOCK
#define VMFS_BLOCK

/* Block types */
enum {
   VMFS_BLK_TYPE_INVALID = 0,
   VMFS_BLK_TYPE_FB,     /* Full Block */
   VMFS_BLK_TYPE_SB,     /* Sub-Block */
   VMFS_BLK_TYPE_PB,     /* Pointer Block */
   VMFS_BLK_TYPE_FD,     /* File Descriptor */
   VMFS_BLK_TYPE_MAX,
};

/* Extract block type from a block ID */
#define VMFS_BLK_TYPE(blk_id)  ((blk_id) & 0x07)

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

/* Initialize an empty block list */
void vmfs_blk_list_init(vmfs_blk_list_t *list);

/* Add a new block at tail of a block list */
int vmfs_blk_list_add_block(vmfs_blk_list_t *list,m_u32_t blk_id);

/* Get a block ID from a block list, given its position */
int vmfs_blk_list_get_block(vmfs_blk_list_t *list,u_int pos,m_u32_t *blk_id);

#endif
