#ifndef KERNEL_FAT32_BPB_H
#define KERNEL_FAT32_BPB_H

#include <stdint.h>

#pragma pack(push, 1)

typedef struct {
    uint8_t  jmp_boot[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    /* FAT32 Extended Section */
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} fat32_bpb_t;

typedef struct {
    uint32_t lead_signature;      /* 0x41615252 */
    uint8_t  reserved1[480];
    uint32_t struct_signature;    /* 0x61417272 */
    uint32_t free_count;          /* Son bilinen boş küme sayısı */
    uint32_t next_free;           /* Arama için başlanacak sonraki boş küme */
    uint8_t  reserved2[12];
    uint32_t trail_signature;     /* 0xAA550000 */
} fat32_fsinfo_t;

#pragma pack(pop)

#endif /* KERNEL_FAT32_BPB_H */