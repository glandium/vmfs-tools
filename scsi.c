#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>

/* SCSI "reserve" command */
#define SCSI_CMD_RESERVE      0x16
#define SCSI_CMD_LEN_RESERVE  6

/* SCSI "release command */
#define SCSI_CMD_RELEASE      0x17
#define SCSI_CMD_LEN_RELEASE  6

/* Send a SCSI "reserve" command */
int scsi_reserve(int fd)
{
   sg_io_hdr_t io_hdr;  
   u_char sense_buffer[32];
   u_char cmd[SCSI_CMD_LEN_RESERVE] = { SCSI_CMD_RESERVE, 0, 0, 0, 0, 0 };

   io_hdr.interface_id    = 'S';
   io_hdr.cmd_len         = sizeof(cmd);
   io_hdr.mx_sb_len       = sizeof(sense_buffer);
   io_hdr.dxfer_direction = SG_DXFER_NONE;
   io_hdr.cmdp            = cmd;
   io_hdr.sbp             = sense_buffer;
   io_hdr.timeout         = 5000;
    
   if (ioctl(fd,SG_IO,&io_hdr) < 0) {
      perror("ioctl");
      return(-1);
   }
   
   return(0);
}

/* Send a SCSI "release" command */
int scsi_release(int fd)
{
   sg_io_hdr_t io_hdr;  
   u_char sense_buffer[32];
   u_char cmd[SCSI_CMD_LEN_RELEASE] = { SCSI_CMD_RELEASE, 0, 0, 0, 0, 0 };

   io_hdr.interface_id    = 'S';
   io_hdr.cmd_len         = sizeof(cmd);
   io_hdr.mx_sb_len       = sizeof(sense_buffer);
   io_hdr.dxfer_direction = SG_DXFER_NONE;
   io_hdr.cmdp            = cmd;
   io_hdr.sbp             = sense_buffer;
   io_hdr.timeout         = 5000;
    
   if (ioctl(fd,SG_IO,&io_hdr) < 0) {
      perror("ioctl");
      return(-1);
   }
   
   return(0);
}
