#ifndef VMFS_H
#define VMFS_H

/* VMFS types - forward declarations */
typedef struct vmfs_volinfo vmfs_volinfo_t;
typedef struct vmfs_fsinfo vmfs_fsinfo_t;
typedef struct vmfs_lvminfo vmfs_lvminfo_t;
typedef struct vmfs_heartbeat vmfs_heartbeat_t;
typedef struct vmfs_bitmap_header vmfs_bitmap_header_t;
typedef struct vmfs_bitmap_entry  vmfs_bitmap_entry_t;
typedef struct vmfs_inode vmfs_inode_t;
typedef struct vmfs_dirent vmfs_dirent_t;
typedef struct vmfs_blk_array vmfs_blk_array_t;
typedef struct vmfs_blk_list vmfs_blk_list_t;
typedef struct vmfs_file vmfs_file_t;
typedef struct vmfs_volume vmfs_volume_t;
typedef struct vmfs_fs vmfs_fs_t;
typedef struct vmfs_lvm vmfs_lvm_t;

union vmfs_flags {
   int packed;
   struct {
      unsigned int debug_level:4;
   };
};

typedef union vmfs_flags vmfs_flags_t __attribute__((transparent_union));

#include "utils.h"
#include "vmfs_heartbeat.h"
#include "vmfs_block.h"
#include "vmfs_bitmap.h"
#include "vmfs_inode.h"
#include "vmfs_dirent.h"
#include "vmfs_file.h"
#include "vmfs_volume.h"
#include "vmfs_fs.h"
#include "vmfs_lvm.h"

#endif
