#ifndef VMFS_INODE_H
#define VMFS_INODE_H

#define VMFS_INODE_SIZE  0x800

#define VMFS_INODE_OFS_GRP_ID     0x0000
#define VMFS_INODE_OFS_POS        0x0004
#define VMFS_INODE_OFS_HB_POS     0x000c
#define VMFS_INODE_OFS_HB_LOCK    0x0024
#define VMFS_INODE_OFS_HB_UUID    0x0028
#define VMFS_INODE_OFS_ID         0x0200
#define VMFS_INODE_OFS_ID2        0x0204
#define VMFS_INODE_OFS_TYPE       0x020c
#define VMFS_INODE_OFS_SIZE       0x0214
#define VMFS_INODE_OFS_TS1        0x022c
#define VMFS_INODE_OFS_TS2        0x0230
#define VMFS_INODE_OFS_TS3        0x0234
#define VMFS_INODE_OFS_UID        0x0238
#define VMFS_INODE_OFS_GID        0x023c
#define VMFS_INODE_OFS_MODE       0x0240

#define VMFS_INODE_OFS_BLK_ARRAY  0x0400
#define VMFS_INODE_BLK_COUNT      0x100

struct vmfs_inode {
   m_u32_t group_id;
   m_u64_t position;
   m_u64_t hb_pos;
   m_u32_t hb_lock;
   uuid_t  hb_uuid;
   m_u32_t id,id2;
   m_u32_t type;
   m_u64_t size;
   m_u32_t ts1,ts2,ts3;
   m_u32_t uid,gid;
   m_u32_t mode;
};

/* Read an inode */
int vmfs_inode_read(vmfs_inode_t *inode,u_char *buf);

/* Show an inode */
void vmfs_inode_show(vmfs_inode_t *inode);

/* Get the offset corresponding to an inode in the FDC file */
off_t vmfs_inode_get_offset(vmfs_volume_t *vol,m_u32_t blk_id);

/* Get inode associated to a directory entry */
int vmfs_inode_get(vmfs_volume_t *vol,vmfs_dirent_t *rec,u_char *buf);

/* Bind inode info to a file */
int vmfs_inode_bind(vmfs_file_t *f,u_char *inode_buf);

#endif
