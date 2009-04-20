/*
 * VMFS prototype (based on http://code.google.com/p/vmfs/ from fluidOps)
 * C.Fillot, 2009/04/15
 */

#include "vmfs.h"

/* "cat" command */
static int cmd_cat(vmfs_volume_t *vol,int argc,char *argv[])
{
   vmfs_file_t *f;
   int i;

   if (argc == 0) {
      printf("Usage: cat file1 ... fileN\n");
      return(-1);
   }

   for(i=0;i<argc;i++) {
      if (!(f = vmfs_file_open(vol,argv[i]))) {
         fprintf(stderr,"Unable to open file %s\n",argv[i]);
         return(-1);
      }

      vmfs_file_dump(f,0,0,stdout);
      vmfs_file_close(f);
   }

   return(0);
}

/* Show a directory entry */
static int cmd_show_dirent(vmfs_volume_t *vol,int argc,char *argv[])
{
   vmfs_dirent_t entry;

   if (argc == 0) {
      printf("Usage: show_dirent <filename>\n");
      return(-1);
   }

   if (vmfs_dirent_resolve_path(vol,vol->root_dir,argv[0],&entry) != 1) {
      fprintf(stderr,"Unable to resolve path '%s'\n",argv[0]);
      return(-1);
   }

   vmfs_dirent_show(&entry);
   return(0);
}

/* Show an inode */
static int cmd_show_inode(vmfs_volume_t *vol,int argc,char *argv[])
{
   vmfs_file_t *f;

   if (argc == 0) {
      printf("Usage: show_dirent <filename>\n");
      return(-1);
   }

   if (!(f = vmfs_file_open(vol,argv[0]))) {
      fprintf(stderr,"Unable to open file '%s'\n",argv[0]);
      return(-1);
   }

   vmfs_inode_show(&f->inode);
   vmfs_file_close(f);
   return(0);
}

/* Show file blocks */
static int cmd_show_file_blocks(vmfs_volume_t *vol,int argc,char *argv[])
{
   vmfs_file_t *f;

   if (argc == 0) {
      printf("Usage: show_file_blocks <filename>\n");
      return(-1);
   }

   if (!(f = vmfs_file_open(vol,argv[0]))) {
      fprintf(stderr,"Unable to open file '%s'\n",argv[0]);
      return(-1);
   }

   vmfs_blk_list_show(&f->blk_list);
   vmfs_file_close(f);
   return(0);
}

/* Show volume information */
static int cmd_show_volume(vmfs_volume_t *vol,int argc,char *argv[])
{
   vmfs_volinfo_show(&vol->vol_info);
   vmfs_fsinfo_show(&vol->fs_info);
   return(0);
}

/* Show volume bitmaps */
static int cmd_show_vol_bitmaps(vmfs_volume_t *vol,int argc,char *argv[])
{
   return(vmfs_vol_dump_bitmaps(vol));
}

/* Check volume bitmaps */
static int cmd_check_vol_bitmaps(vmfs_volume_t *vol,int argc,char *argv[])
{
   int errors = 0;

   printf("Checking FBB bitmaps...\n");
   errors += vmfs_bitmap_check(vol->fbb,&vol->fbb_bmh);

   printf("Checking FDC bitmaps...\n");
   errors += vmfs_bitmap_check(vol->fdc,&vol->fdc_bmh);

   printf("Checking PBC bitmaps...\n");
   errors += vmfs_bitmap_check(vol->pbc,&vol->pbc_bmh);

   printf("Checking SBC bitmaps...\n");   
   errors += vmfs_bitmap_check(vol->sbc,&vol->sbc_bmh);

   printf("Total errors: %d\n",errors);
   return(errors);
}

/* Show active heartbeats */
static int cmd_show_heartbeats(vmfs_volume_t *vol,int argc,char *argv[])
{
   return(vmfs_heartbeat_show_active(vol));
}

struct cmd {
   char *name;
   char *description;
   int (*fn)(vmfs_volume_t *vol,int argc,char *argv[]);
};

struct cmd cmd_array[] = {
   { "cat", "Concatenate files and print on standard output", cmd_cat },
   { "show_dirent", "Show directory entry", cmd_show_dirent },
   { "show_inode", "Show inode", cmd_show_inode },
   { "show_file_blocks", "Show file blocks", cmd_show_file_blocks },
   { "show_volume", "Show volume general info", cmd_show_volume },
   { "show_vol_bitmaps", "Show volume bitmaps", cmd_show_vol_bitmaps },
   { "check_vol_bitmaps", "Check volume bitmaps", cmd_check_vol_bitmaps },
   { "show_heartbeats", "Show active heartbeats", cmd_show_heartbeats },
   { NULL, NULL },
};

static void show_usage(char *prog_name) 
{
   int i;

   printf("Syntax: %s <device_name> <command> <args...>\n\n",prog_name);
   printf("Available commands:\n");

   for(i=0;cmd_array[i].name;i++)
      printf("  - %s : %s\n",cmd_array[i].name,cmd_array[i].description);

   printf("\n");
}

static struct cmd *cmd_find(char *name)
{
   int i;

   for(i=0;cmd_array[i].name;i++)
      if (!strcmp(cmd_array[i].name,name))
         return(&cmd_array[i]);

   return NULL;
}

int main(int argc,char *argv[])
{
   vmfs_volume_t *vol;
   char *vol_name,*cmd_name;
   struct cmd *cmd;

   if (argc < 3) {
      show_usage(argv[0]);
      return(0);
   }
   
   vol_name = argv[1];
   cmd_name = argv[2];

   if (!(cmd = cmd_find(cmd_name))) {
      fprintf(stderr,"Unknown command '%s'.\n",cmd_name);
      show_usage(argv[0]);
      return(0);
   }

   if (!(vol = vmfs_vol_create(vol_name,0))) {
      fprintf(stderr,"Unable to open device/file \"%s\".\n",vol_name);
      exit(EXIT_FAILURE);
   }
   
   if (vmfs_vol_open(vol) == -1) {
      fprintf(stderr,"Unable to open volume.\n");
      exit(EXIT_FAILURE);
   }

   return(cmd->fn(vol,argc-3,&argv[3]));
}
