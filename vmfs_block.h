#ifndef VMFS_BLOCK_H
#define VMFS_BLOCK_H

/* Block types */
enum {
   VMFS_BLK_TYPE_COW = 0,/* Copy-on-write for sparse files */
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
struct vmfs_blk_list {
   m_u32_t total,last_pos;
   m_u32_t *blk_id;
};

/* Initialize a block list */
int vmfs_blk_list_init(vmfs_blk_list_t *list,m_u32_t blk_count);

/* Free a block list */
void vmfs_blk_list_free(vmfs_blk_list_t *list);

/* Set a block at the specified position */
int vmfs_blk_list_add_block(vmfs_blk_list_t *list,u_int pos,m_u32_t blk_id);

/* Get a block ID from a block list, given its position */
int vmfs_blk_list_get_block(vmfs_blk_list_t *list,u_int pos,m_u32_t *blk_id);

/* Show a block list */
void vmfs_blk_list_show(vmfs_blk_list_t *list);

#endif
