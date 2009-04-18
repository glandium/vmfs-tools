/*
 * VMFS prototype (based on http://code.google.com/p/vmfs/ from fluidOps)
 * C.Fillot, 2009/04/15
 */

#include "vmfs.h"

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

#if 0
   {
      vmfs_file_t *f;
      FILE *fd;

      fd = fopen("test.vmx","w");
      f = vmfs_file_open(vol,"Test1/Test1.vmx");
      vmfs_file_dump(f,0,0,fd);
      fclose(fd);
   }
#endif

#if 0
   {
      m_u32_t count;
      
      count = vmfs_bitmap_allocated_items(vol->fbb,&vol->fbb_bmh);
      printf("FBB allocated items: %d (0x%x)\n",count,count);
      vmfs_bitmap_check(vol->fbb,&vol->fbb_bmh);

      count = vmfs_bitmap_allocated_items(vol->fdc,&vol->fdc_bmh);
      printf("FDC allocated items: %d (0x%x)\n",count,count);
      vmfs_bitmap_check(vol->fdc,&vol->fdc_bmh);

      count = vmfs_bitmap_allocated_items(vol->pbc,&vol->pbc_bmh);
      printf("PBC allocated items: %d (0x%x)\n",count,count);
      vmfs_bitmap_check(vol->pbc,&vol->pbc_bmh);

      count = vmfs_bitmap_allocated_items(vol->sbc,&vol->sbc_bmh);
      printf("SBC allocated items: %d (0x%x)\n",count,count);
      vmfs_bitmap_check(vol->sbc,&vol->sbc_bmh);

   }
#endif

#if 1
   vmfs_heartbeat_show_active(vol);
#endif

   return(0);
}
