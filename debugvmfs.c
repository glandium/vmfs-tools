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
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include "vmfs.h"
#include "readcmd.h"

/* "cat" command */
static int cmd_cat(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_file_t *f;
   int i;

   if (argc == 0) {
      fprintf(stderr,"Usage: cat file1 ... fileN\n");
      return(-1);
   }

   for(i=0;i<argc;i++) {
      if (!(f = vmfs_file_open_from_path(fs,argv[i]))) {
         fprintf(stderr,"Unable to open file %s\n",argv[i]);
         return(-1);
      }

      vmfs_file_dump(f,0,0,stdout);
      vmfs_file_close(f);
   }

   return(0);
}

/* "ls" command */
static int cmd_ls(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_dirent_t **dlist,*entry;
   struct stat st_info;
   struct passwd *usr;
   struct group *grp;
   char buffer[1024];
   int i,res,long_format=0;

   if ((argc == 2) && (strcmp(argv[0],"-l") == 0)) {
      long_format = 1;
      argv++;
      argc--;
   }

   if (argc != 1) {
      fprintf(stderr,"Usage: ls [-l] <path>\n");
      return(-1);
   }


   res = vmfs_dirent_readdir(fs,argv[0],&dlist);

   if (res == -1) {
      fprintf(stderr,"Unable to read directory %s\n",argv[0]);
      return(-1);
   }

   for(i=0;i<res;i++) {
      entry = dlist[i];
      if (long_format) {
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
      } else {
         printf("%s\n",entry->name);
      }
   }

   vmfs_dirent_free_dlist(res,&dlist);
   return(0);
}

/* "df" (disk free) command */
static int cmd_df(vmfs_fs_t *fs,int argc,char *argv[])
{
   uint32_t alloc,total;

   total = fs->fbb->bmh.total_items;
   alloc = vmfs_bitmap_allocated_items(fs->fbb);

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
      fprintf(stderr,"Usage: show_dirent <filename>\n");
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
      fprintf(stderr,"Usage: show_dirent <filename>\n");
      return(-1);
   }

   if (!(f = vmfs_file_open_from_path(fs,argv[0]))) {
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
      fprintf(stderr,"Usage: show_file_blocks <filename>\n");
      return(-1);
   }

   if (!(f = vmfs_file_open_from_path(fs,argv[0]))) {
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
   errors += vmfs_bitmap_check(fs->fbb);

   printf("Checking FDC bitmaps...\n");
   errors += vmfs_bitmap_check(fs->fdc);

   printf("Checking PBC bitmaps...\n");
   errors += vmfs_bitmap_check(fs->pbc);

   printf("Checking SBC bitmaps...\n");   
   errors += vmfs_bitmap_check(fs->sbc);

   printf("Total errors: %d\n",errors);
   return(errors);
}

/* Show active heartbeats */
static int cmd_show_heartbeats(vmfs_fs_t *fs,int argc,char *argv[])
{
   return(vmfs_heartbeat_show_active(fs));
}

/* Test heartbeats */
static int cmd_test_heartbeats(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_heartbeat_t hb;
   u_int id;

   if (argc == 0) {
      fprintf(stderr,"Usage: test_heartbeats <hb_id>\n");
      return(-1);
   }

   id = atoi(argv[0]);

   printf("Initial status (Before vmfs_heartbeat_lock()):\n");
   vmfs_heartbeat_show_active(fs);

   /* Test lock function */
   vmfs_heartbeat_lock(fs,id,&hb);

   printf("After vmfs_heartbeat_lock():\n");
   vmfs_heartbeat_show_active(fs);

   /* Test update function */
   sleep(1);
   vmfs_heartbeat_update(fs,&hb);

   printf("After vmfs_heartbeat_update():\n");
   vmfs_heartbeat_show_active(fs);

   /* Test unlock function */
   vmfs_heartbeat_unlock(fs,&hb);

   printf("After vmfs_heartbeat_unlock():\n");
   vmfs_heartbeat_show_active(fs);

   return(0);
}

/* Convert a raw block ID in human readable form */
static int cmd_convert_block_id(vmfs_fs_t *fs,int argc,char *argv[])
{
   uint32_t blk_id,blk_type;
   int i;

   if (argc == 0) {
      fprintf(stderr,"Usage: convert_block_id blk1 ... blkN\n");
      return(-1);
   }
   
   for(i=0;i<argc;i++) {
      blk_id = (uint32_t)strtoul(argv[i],NULL,16);
      blk_type = VMFS_BLK_TYPE(blk_id);

      printf("Block ID 0x%8.8x: ",blk_id);
      
      switch(blk_type) {
         case VMFS_BLK_TYPE_FB:
            printf("Full-Block, Item=0x%8.8x\n",VMFS_BLK_FB_ITEM(blk_id));
            break;

         case VMFS_BLK_TYPE_SB:
            printf("Sub-Block, Entry=0x%8.8x, Item=0x%2.2x\n",
                   VMFS_BLK_SB_ENTRY(blk_id),VMFS_BLK_SB_ITEM(blk_id));
            break;

         case VMFS_BLK_TYPE_PB:
            printf("Pointer-Block, Entry=0x%8.8x, Item=0x%2.2x\n",
                   VMFS_BLK_PB_ENTRY(blk_id),VMFS_BLK_PB_ITEM(blk_id));
            break;

         case VMFS_BLK_TYPE_FD:
            printf("File Descriptor, Entry=0x%4.4x, Item=0x%3.3x\n",
                   VMFS_BLK_FD_ENTRY(blk_id),VMFS_BLK_FD_ITEM(blk_id));
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
   uint32_t blk_id,blk_type;
   uint64_t blk_size;
   u_char *buf;

   if (argc == 0) {
      fprintf(stderr,"Usage: read_block blk_id\n");
      return(-1);
   }

   blk_size = vmfs_fs_get_blocksize(fs);

   if (posix_memalign((void **)&buf,M_SECTOR_SIZE,blk_size))
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
         vmfs_bitmap_get_item(fs->sbc,VMFS_BLK_SB_ENTRY(blk_id),
                              VMFS_BLK_SB_ITEM(blk_id),buf);
         mem_dump(stdout,buf,fs->sbc->bmh.data_size);
         break;

      default:
         fprintf(stderr,"Unsupported block type 0x%2.2x\n",blk_type);
   }

   free(buf);
   return(0);
}

/* Show a bitmap item */
static int cmd_show_bitmap_item(vmfs_fs_t *fs,int argc,char *argv[])
{   
   uint32_t blk_id,blk_type;
   uint32_t entry,item;
   vmfs_bitmap_t *bmp;
   u_char *buf;
   size_t len;

   if (argc == 0) {
      fprintf(stderr,"Usage: show_bitmap_item blk_id\n");
      return(-1);
   }

   blk_id = (uint32_t)strtoul(argv[0],NULL,16);
   blk_type = VMFS_BLK_TYPE(blk_id);

   switch(blk_type) {
      case VMFS_BLK_TYPE_PB:
         bmp   = fs->pbc;
         entry = VMFS_BLK_PB_ENTRY(blk_id);
         item  = VMFS_BLK_PB_ITEM(blk_id);
         break;
      case VMFS_BLK_TYPE_SB:
         bmp   = fs->sbc;
         entry = VMFS_BLK_SB_ENTRY(blk_id);
         item  = VMFS_BLK_SB_ITEM(blk_id);
         break;
      case VMFS_BLK_TYPE_FD:
         bmp = fs->fdc;
         entry = VMFS_BLK_FD_ENTRY(blk_id);
         item  = VMFS_BLK_FD_ITEM(blk_id);
         break;
      default:
         fprintf(stderr,"Unsupported block type 0x%2.2x\n",blk_type);
         return(-1);
   }

   len = bmp->bmh.data_size;
   buf = malloc(len);

   vmfs_bitmap_get_item(bmp,entry,item,buf);
   mem_dump(stdout,buf,len);
   free(buf);
   return(0);
}

/* Show host information */
static int cmd_show_host(vmfs_fs_t *fs,int argc,char *argv[])
{
   vmfs_host_show_info();
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
   { "ls", "List files in specified directory", cmd_ls },
   { "df", "Show available free space", cmd_df },
   { "show_dirent", "Show directory entry", cmd_show_dirent },
   { "show_inode", "Show inode", cmd_show_inode },
   { "show_file_blocks", "Show file blocks", cmd_show_file_blocks },
   { "show_fs", "Show file system info", cmd_show_fs },
   { "show_volume", "Show volume general info", cmd_show_volume },
   { "show_vol_bitmaps", "Show volume bitmaps", cmd_show_vol_bitmaps },
   { "check_vol_bitmaps", "Check volume bitmaps", cmd_check_vol_bitmaps },
   { "show_heartbeats", "Show active heartbeats", cmd_show_heartbeats },
   { "test_heartbeats", "Test heartbeats", cmd_test_heartbeats },
   { "convert_block_id", "Convert block ID", cmd_convert_block_id },
   { "read_block", "Read a block", cmd_read_block },
   { "show_bitmap_item", "Show a bitmap item", cmd_show_bitmap_item },
   { "show_host", "Show host information", cmd_show_host },
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

/* Executes a command through "sh -c", and returns the file descriptor to its
 * stdin or -1 on error */
static int pipe_exec(const char *cmd) {
   int fd[2];
   pid_t p;

   if (pipe(fd) == -1)
      return -1;

   if ((p = fork()) == -1)
      return -1;

   if (p == 0) { /* Child process */
      close(fd[1]);
      dup2(fd[0],0);
      close(fd[0]);
      execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
      return -1;
   } else { /* Parent process */
      close(fd[0]);
      return fd[1];
   }
}

/* Opens a shell */
static int cmd_shell(vmfs_fs_t *fs,int argc,char *argv[])
{
   struct cmd *cmd = NULL;
   const cmd_t *cmdline = NULL;
   do {
      freecmd(cmdline);
      cmdline = readcmd("debugvmfs> ");
      if (!cmdline) return(0);
      if (!cmdline->argc) continue;
      if (!strcmp(cmdline->argv[0], "exit") ||
          !strcmp(cmdline->argv[0], "quit")) return(0);

      cmd = cmd_find(cmdline->argv[0]);
      if (!cmd) {
        int i;
        printf("Unknown command: %s\n", cmdline->argv[0]);
        printf("Available commands:\n");
        for(i=0;cmd_array[i].name;i++)
           if (cmd_array[i].fn != cmd_shell)
              printf("  - %s : %s\n",cmd_array[i].name,cmd_array[i].description);
      } else if (cmd->fn != cmd_shell) {
        int out = -1;
        if (cmdline->redir) {
           int fd;
           if (cmdline->piped) {
              if ((fd = pipe_exec(cmdline->redir)) < 0) {
                 fprintf(stderr, "Error executing pipe command: %s\n",
                         strerror(errno));
                 continue;
              }
           } else if ((fd = open(cmdline->redir,O_CREAT|O_WRONLY|
                                 (cmdline->append?O_APPEND:O_TRUNC),
                                 0666)) < 0) {
              fprintf(stderr, "Error opening %s: %s\n",cmdline->redir,
                                                       strerror(errno));
              continue;
           }
           out=dup(1);
           dup2(fd,1);
           close(fd);
        }
        cmd->fn(fs,cmdline->argc-1,&cmdline->argv[1]);
        if (cmdline->redir) {
           dup2(out,1);
           close(out);
           if (cmdline->piped) wait(NULL);
        }
      }
   } while (1);
}

int main(int argc,char *argv[])
{
   vmfs_lvm_t *lvm;
   vmfs_fs_t *fs;
   struct cmd *cmd = NULL;
   int arg, i, ret;
   vmfs_flags_t flags;

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

   vmfs_host_init();

   flags.packed = 0;

#ifdef VMFS_WRITE
   flags.read_write = 1;
#endif

   if (!(lvm = vmfs_lvm_create(flags))) {
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
