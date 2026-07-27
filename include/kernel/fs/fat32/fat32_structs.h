#ifndef KERNEL_FAT32_STRUCTS_H
#define KERNEL_FAT32_STRUCTS_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel/fs/fat32/fat32_bpb.h"

#define FAT32_EOF                0x0FFFFFF8
#define FAT32_BAD_CLUSTER        0x0FFFFFF7
#define FAT32_FREE_CLUSTER       0x00000000

#define FAT_ATTR_READ_ONLY       0x01
#define FAT_ATTR_HIDDEN          0x02
#define FAT_ATTR_SYSTEM          0x04
#define FAT_ATTR_VOLUME_ID       0x08
#define FAT_ATTR_DIRECTORY       0x10
#define FAT_ATTR_ARCHIVE         0x20
#define FAT_ATTR_LFN             (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)

#define LFN_LAST_ENTRY_MASK      0x40

#pragma pack(push, 1)

typedef struct {
    char     name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat32_dir_entry_t;

typedef struct {
    uint8_t  sequence_number;
    uint16_t name1[5];
    uint8_t  attr;               /* Always FAT_ATTR_LFN (0x0F) */
    uint8_t  type;               /* Always 0x00 */
    uint8_t  checksum;           /* Short Name Checksum */
    uint16_t name2[6];
    uint16_t zero;               /* Always 0x0000 */
    uint16_t name3[2];
} fat32_lfn_entry_t;

#pragma pack(pop)

typedef struct {
    uint32_t      block_dev_id;
    fat32_bpb_t   bpb;
    uint32_t      fat_start_lba;
    uint32_t      cluster_heap_lba;
    uint32_t      sectors_per_cluster;
    uint32_t      bytes_per_cluster;
    uint32_t      root_cluster;
    uint32_t      total_clusters;
} fat32_fs_t;

#endif /* KERNEL_FAT32_STRUCTS_H */