/* 
 * VMFS inodes.
 */

#include "vmfs.h"

/* Read an inode */
int vmfs_inode_read(vmfs_inode_t *inode,u_char *buf)
{
   inode->group_id = read_le32(buf,VMFS_INODE_OFS_GRP_ID);
   inode->position = read_le64(buf,VMFS_INODE_OFS_POS);
   inode->hb_pos   = read_le64(buf,VMFS_INODE_OFS_HB_POS);
   inode->hb_lock  = read_le32(buf,VMFS_INODE_OFS_HB_LOCK);
   inode->id       = read_le32(buf,VMFS_INODE_OFS_ID);
   inode->id2      = read_le32(buf,VMFS_INODE_OFS_ID2);
   inode->type     = read_le32(buf,VMFS_INODE_OFS_TYPE);
   inode->size     = read_le64(buf,VMFS_INODE_OFS_SIZE);
   inode->ts1      = read_le32(buf,VMFS_INODE_OFS_TS1);
   inode->ts2      = read_le32(buf,VMFS_INODE_OFS_TS2);
   inode->ts3      = read_le32(buf,VMFS_INODE_OFS_TS3);
   inode->uid      = read_le32(buf,VMFS_INODE_OFS_UID);
   inode->gid      = read_le32(buf,VMFS_INODE_OFS_GID);
   inode->mode     = read_le32(buf,VMFS_INODE_OFS_MODE);

   memcpy(inode->hb_uuid,buf+VMFS_INODE_OFS_HB_UUID,sizeof(inode->hb_uuid));
   return(0);
}

/* Show an inode */
void vmfs_inode_show(vmfs_inode_t *inode)
{
   printf("  - Group ID    : 0x%8.8x\n",inode->group_id);
   printf("  - Position    : 0x%llx\n",inode->position);
   printf("  - HB Position : 0x%llx\n",inode->hb_pos);
   printf("  - HB Lock     : %d (%s)\n",
          inode->hb_lock,(inode->hb_lock > 0) ? "LOCKED":"UNLOCKED");
   printf("  - ID          : 0x%8.8x\n",inode->id);
   printf("  - ID2         : 0x%8.8x\n",inode->id2);
   printf("  - Type        : 0x%8.8x\n",inode->type);
   printf("  - Size        : 0x%8.8llx\n",inode->size);
   printf("  - UID/GID     : %d/%d\n",inode->uid,inode->gid);
   printf("  - Mode        : 0%o\n",inode->mode);
}

/* Get the offset corresponding to an inode in the FDC file */
off_t vmfs_inode_get_offset(vmfs_volume_t *vol,m_u32_t blk_id)
{
   m_u32_t subgroup,number;
   off_t inode_addr;
   m_u32_t fdc_blk;

   subgroup = VMFS_BLK_FD_SUBGROUP(blk_id);
   number   = VMFS_BLK_FD_NUMBER(blk_id);

   /* Compute the address of the file meta-info in the FDC file */
   fdc_blk = subgroup * vol->fdc_bmh.items_per_bitmap_entry;
   inode_addr  = vmfs_bitmap_get_block_addr(&vol->fdc_bmh,fdc_blk);
   inode_addr += number * vol->fdc_bmh.data_size;

   return(inode_addr);
}

/* Get inode associated to a directory entry */
int vmfs_inode_get(vmfs_volume_t *vol,vmfs_dirent_t *rec,u_char *buf)
{
   m_u32_t blk_id = rec->block_id;
   off_t inode_addr;
   ssize_t len;

   if (VMFS_BLK_TYPE(blk_id) != VMFS_BLK_TYPE_FD)
      return(-1);

   inode_addr = vmfs_inode_get_offset(vol,blk_id);

   if (vmfs_file_seek(vol->fdc,inode_addr,SEEK_SET) == -1)
      return(-1);
   
   len = vmfs_file_read(vol->fdc,buf,vol->fdc_bmh.data_size);
   return((len == vol->fdc_bmh.data_size) ? 0 : -1);
}

/* Resolve pointer blocks */
static int vmfs_inode_resolve_pb(vmfs_file_t *f,m_u32_t blk_id)
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

/* Bind inode info to a file */
int vmfs_inode_bind(vmfs_file_t *f,u_char *inode_buf)
{
   m_u32_t blk_id,blk_type;
   int i;

   vmfs_inode_read(&f->inode,inode_buf);
   vmfs_blk_list_init(&f->blk_list);

   for(i=0;i<VMFS_INODE_BLK_COUNT;i++) {
      blk_id   = read_le32(inode_buf,VMFS_INODE_OFS_BLK_ARRAY+(i*4));
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
            if (vmfs_inode_resolve_pb(f,blk_id) == -1) {
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
