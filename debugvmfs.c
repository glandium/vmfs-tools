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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "vmfs.h"

/* "cat" command */
static int cmd_cat(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_file_t *f;
   int i;

   if (argc == 0) {
      printf("Usage: cat file1 ... fileN\n");
      return(-1);
   }

   for(i=0;i<argc;i++) {
      if (!(f = vmfs_file_open(fs,argv[i]))) {
         fprintf(stderr,"Unable to open file %s\n",argv[i]);
         return(-1);
      }

      vmfs_file_dump(f,0,0,stdout);
      vmfs_file_close(f);
   }

   return(0);
}

/* "dir" command */
static int cmd_dir(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_dirent_t **dlist,*entry;
   int i,res;

   if (argc == 0) {
      printf("Usage: dir <path>\n");
      return(-1);
   }

   res = vmfs_dirent_readdir(fs,argv[0],&dlist);

   if (res == -1) {
      fprintf(stderr,"Unable to read directory %s\n",argv[0]);
      return(-1);
   }

   for(i=0;i<res;i++) {
      entry = dlist[i]; 
      printf("%s\n",entry->name);
   }

   vmfs_dirent_free_dlist(res,&dlist);
   return(0);
}

/* "dir" with long format command */
static int cmd_dirl(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_dirent_t **dlist,*entry;
   struct stat st_info;
   struct passwd *usr;
   struct group *grp;
   char buffer[1024];
   int i,res;

   if (argc == 0) {
      printf("Usage: dir <path>\n");
      return(-1);
   }

   res = vmfs_dirent_readdir(fs,argv[0],&dlist);

   if (res == -1) {
      fprintf(stderr,"Unable to read directory %s\n",argv[0]);
      return(-1);
   }

   for(i=0;i<res;i++) {
      entry = dlist[i];

      snprintf(buffer,sizeof(buffer),"%s/%s",argv[0],entry->name);
      if (vmfs_file_lstat(fs,buffer,&st_info) == -1)
         continue;

      printf("%-10s ",m_fmode_to_str(st_info.st_mode,buffer));

      usr = getpwuid(st_info.st_uid);
      grp = getgrgid(st_info.st_gid);

      printf("%u ", (unsigned int)st_info.st_nlink);
      if (usr)
         printf("%8s ", usr->pw_name);
      else
         printf("%8u ", st_info.st_uid);
      if (grp)
         printf("%8s ", grp->gr_name);
      else
         printf("%8u ", st_info.st_gid);

      printf("%10llu %s %s\n",
             (unsigned long long)st_info.st_size,
             m_ctime(&st_info.st_ctime,buffer,sizeof(buffer)),
             entry->name);
   }

   vmfs_dirent_free_dlist(res,&dlist);
   return(0);
}

/* "df" (disk free) command */
static int cmd_df(vmfs_fs_t *fs,int argc,char *argv[])
{
   uint32_t alloc,total;

   total = fs->fbb_bmh.total_items;
   alloc = vmfs_bitmap_allocated_items(fs->fbb,&fs->fbb_bmh);

   printf("Block size       : %"PRIu64" bytes\n",vmfs_fs_get_blocksize(fs));

   printf("Total blocks     : %u\n",total);
   printf("Total size       : %"PRIu64" Mb\n",
          (vmfs_fs_get_blocksize(fs)*total)/1048576);

   printf("Allocated blocks : %u\n",alloc);
   printf("Allocated space  : %"PRIu64" Mb\n",
          (vmfs_fs_get_blocksize(fs)*alloc)/1048576);

   printf("Free blocks      : %u\n",total-alloc);
   printf("Free size        : %"PRIu64" Mb\n",
          (vmfs_fs_get_blocksize(fs)*(total-alloc))/1048576);

   return(0);
}

/* Show a directory entry */
static int cmd_show_dirent(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_dirent_t entry;

   if (argc == 0) {
      printf("Usage: show_dirent <filename>\n");
      return(-1);
   }

   if (vmfs_dirent_resolve_path(fs,fs->root_dir,argv[0],0,&entry) != 1) {
      fprintf(stderr,"Unable to resolve path '%s'\n",argv[0]);
      return(-1);
   }

   vmfs_dirent_show(&entry);
   return(0);
}

/* Show an inode */
static int cmd_show_inode(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_file_t *f;

   if (argc == 0) {
      printf("Usage: show_dirent <filename>\n");
      return(-1);
   }

   if (!(f = vmfs_file_open(fs,argv[0]))) {
      fprintf(stderr,"Unable to open file '%s'\n",argv[0]);
      return(-1);
   }

   vmfs_inode_show(&f->inode);
   vmfs_file_close(f);
   return(0);
}

/* Show file blocks */
static int cmd_show_file_blocks(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_file_t *f;

   if (argc == 0) {
      printf("Usage: show_file_blocks <filename>\n");
      return(-1);
   }

   if (!(f = vmfs_file_open(fs,argv[0]))) {
      fprintf(stderr,"Unable to open file '%s'\n",argv[0]);
      return(-1);
   }

   vmfs_blk_list_show(&f->blk_list);
   vmfs_file_close(f);
   return(0);
}

/* Show filesystem information */
static int cmd_show_fs(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_fs_show(fs);
   return(0);
}

/* Show volume information */
static int cmd_show_volume(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_lvm_show(fs->lvm);
   return(0);
}

/* Show volume bitmaps */
static int cmd_show_vol_bitmaps(vmfs_fs_t *fs,int argc,char *argv[])
{
   return(vmfs_fs_dump_bitmaps(fs));
}

/* Check volume bitmaps */
static int cmd_check_vol_bitmaps(vmfs_fs_t *fs,int argc,char *argv[])
{
   int errors = 0;

   printf("Checking FBB bitmaps...\n");
   errors += vmfs_bitmap_check(fs->fbb,&fs->fbb_bmh);

   printf("Checking FDC bitmaps...\n");
   errors += vmfs_bitmap_check(fs->fdc,&fs->fdc_bmh);

   printf("Checking PBC bitmaps...\n");
   errors += vmfs_bitmap_check(fs->pbc,&fs->pbc_bmh);

   printf("Checking SBC bitmaps...\n");   
   errors += vmfs_bitmap_check(fs->sbc,&fs->sbc_bmh);

   printf("Total errors: %d\n",errors);
   return(errors);
}

/* Show active heartbeats */
static int cmd_show_heartbeats(vmfs_fs_t *fs,int argc,char *argv[])
{
   return(vmfs_heartbeat_show_active(fs));
}

/* Convert a raw block ID in human readable form */
static int cmd_convert_block_id(vmfs_fs_t *fs,int argc,char *argv[])
{
   uint32_t blk_id,blk_type;
   int i;

   if (argc == 0) {
      printf("Usage: convert_block_id blk1 ... blkN\n");
      return(-1);
   }
   
   for(i=0;i<argc;i++) {
      blk_id = (uint32_t)strtoul(argv[i],NULL,16);
      blk_type = VMFS_BLK_TYPE(blk_id);

      printf("Block ID 0x%8.8x: ",blk_id);
      
      switch(blk_type) {
         case VMFS_BLK_TYPE_FB:
            printf("Full-Block, Number=0x%8.8x\n",VMFS_BLK_FB_ITEM(blk_id));
            break;

         case VMFS_BLK_TYPE_SB:
            printf("Sub-Block, Number=0x%8.8x, Subgroup=0x%2.2x\n",
                   VMFS_BLK_SB_ITEM(blk_id),VMFS_BLK_SB_ENTRY(blk_id));
            break;

         case VMFS_BLK_TYPE_PB:
            printf("Pointer-Block, Number=0x%8.8x, Subgroup=0x%2.2x\n",
                   VMFS_BLK_PB_ITEM(blk_id),VMFS_BLK_PB_ENTRY(blk_id));
            break;

         case VMFS_BLK_TYPE_FD:
            printf("File Descriptor, Number=0x%8.8x, Subgroup=0x%2.2x\n",
                   VMFS_BLK_FD_ITEM(blk_id),VMFS_BLK_FD_ENTRY(blk_id));
            break;

         default:
            printf("Unknown block type 0x%2.2x\n",blk_type);
      }
   };

   return(0);
}

/* Read a block */
static int cmd_read_block(vmfs_fs_t *fs,int argc,char *argv[])
{    
   uint32_t sbc_entry,sbc_item,sbc_blk;
   uint32_t blk_id,blk_type;
   uint64_t blk_size;
   off_t sbc_addr;
   u_char *buf;

   if (argc == 0) {
      printf("Usage: read_block blk_id\n");
      return(-1);
   }

   blk_size = vmfs_fs_get_blocksize(fs);

   if (!(buf = malloc(blk_size)))
      return(-1);

   blk_id = (uint32_t)strtoul(argv[0],NULL,16);
   blk_type = VMFS_BLK_TYPE(blk_id);

   switch(blk_type) {
      /* Full Block */
      case VMFS_BLK_TYPE_FB:
         vmfs_fs_read(fs,VMFS_BLK_FB_ITEM(blk_id),0,buf,blk_size);
         mem_dump(stdout,buf,blk_size);
         break;

      /* Sub-Block */
      case VMFS_BLK_TYPE_SB:
         blk_size = fs->sbc_bmh.data_size;

         sbc_entry = VMFS_BLK_SB_ENTRY(blk_id);
         sbc_item  = VMFS_BLK_SB_ITEM(blk_id);

         sbc_blk = sbc_entry * fs->sbc_bmh.items_per_bitmap_entry;
         sbc_blk += sbc_item;
         
         sbc_addr = vmfs_bitmap_get_block_addr(&fs->sbc_bmh,sbc_blk);

         vmfs_file_seek(fs->sbc,sbc_addr,SEEK_SET);
         vmfs_file_read(fs->sbc,buf,blk_size);
         mem_dump(stdout,buf,blk_size);
         break;

      default:
         fprintf(stderr,"Unsupported block type 0x%2.2x\n",blk_type);
   }

   free(buf);
   return(0);
}

struct cmd {
   char *name;
   char *description;
   int (*fn)(vmfs_fs_t *fs,int argc,char *argv[]);
};

/* Opens a shell */
static int cmd_shell(vmfs_fs_t *fs,int argc,char *argv[]);

struct cmd cmd_array[] = {
   { "cat", "Concatenate files and print on standard output", cmd_cat },
   { "dir", "List files in specified directory", cmd_dir },
   { "dirl", "List files in specified directory (long format)", cmd_dirl },
   { "df", "Show available free space", cmd_df },
   { "show_dirent", "Show directory entry", cmd_show_dirent },
   { "show_inode", "Show inode", cmd_show_inode },
   { "show_file_blocks", "Show file blocks", cmd_show_file_blocks },
   { "show_fs", "Show file system info", cmd_show_fs },
   { "show_volume", "Show volume general info", cmd_show_volume },
   { "show_vol_bitmaps", "Show volume bitmaps", cmd_show_vol_bitmaps },
   { "check_vol_bitmaps", "Check volume bitmaps", cmd_check_vol_bitmaps },
   { "show_heartbeats", "Show active heartbeats", cmd_show_heartbeats },
   { "convert_block_id", "Convert block ID", cmd_convert_block_id },
   { "read_block", "Read a block", cmd_read_block },
   { "shell", "Opens a shell", cmd_shell },
   { NULL, NULL },
};

static void show_usage(char *prog_name) 
{
   int i;

   printf("Syntax: %s <device_name...> <command> <args...>\n\n",prog_name);
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

/* Opens a shell */
static int cmd_shell(vmfs_fs_t *fs,int argc,char *argv[])
{
   struct cmd *cmd = NULL;
   char buf[512];
   int aargc;
   char *aargv[256]; /* With a command buffer of 512 bytes, there can't be
                      * more arguments than that */
   int prompt = isatty(fileno(stdin));

   do {
      if (prompt)
         fprintf(stdout, "debugvmfs> ");
      if (!fgets(buf, 511, stdin)) {
         fprintf(stdout, "\n");
         return(0);
      }
      buf[strlen(buf)-1] = 0;
      if (!strcmp(buf, "exit") || !strcmp(buf, "quit"))
         return(0);
      aargc = 0;
      aargv[0] = buf;
      do {
         while (*(aargv[aargc]) == ' ')
            *(aargv[aargc]++) = 0;
      } while((aargv[++aargc] = strchr(aargv[aargc - 1], ' ')));
      cmd = cmd_find(aargv[0]);
      if (!cmd) {
        int i;
        printf("Unknown command: %s\n", aargv[0]);
        printf("Available commands:\n");
        for(i=0;cmd_array[i].name;i++)
           if (cmd_array[i].fn != cmd_shell)
              printf("  - %s : %s\n",cmd_array[i].name,cmd_array[i].description);
      } else if (cmd->fn != cmd_shell) {
        cmd->fn(fs,aargc-1,&aargv[1]);
      }
   } while (1);
}

int main(int argc,char *argv[])
{
   vmfs_lvm_t *lvm;
   vmfs_fs_t *fs;
   struct cmd *cmd = NULL;
   int arg, i, ret;

   if (argc < 3) {
      show_usage(argv[0]);
      return(0);
   }
   
   /* Scan arguments for a command */
   for (arg = 1; arg < argc; arg++) {
      if ((cmd = cmd_find(argv[arg])))
         break;
   }

   if (!cmd) {
      show_usage(argv[0]);
      return(0);
   }

   if (!(lvm = vmfs_lvm_create(0))) {
      fprintf(stderr,"Unable to create LVM structure\n");
      exit(EXIT_FAILURE);
   }

   for (i = 1; i < arg; i++) {
      if (vmfs_lvm_add_extent(lvm,argv[i]) == -1) {
         fprintf(stderr,"Unable to open device/file \"%s\".\n",argv[i]);
         exit(EXIT_FAILURE);
      }
   }

   if (!(fs = vmfs_fs_create(lvm))) {
      fprintf(stderr,"Unable to open filesystem\n");
      exit(EXIT_FAILURE);
   }
   
   if (vmfs_fs_open(fs) == -1) {
      fprintf(stderr,"Unable to open volume.\n");
      exit(EXIT_FAILURE);
   }

   ret = cmd->fn(fs,argc-arg-1,&argv[arg+1]);
   vmfs_fs_close(fs);
   return(ret);
}
