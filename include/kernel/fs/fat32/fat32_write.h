#ifndef KERNEL_FAT32_WRITE_H
#define KERNEL_FAT32_WRITE_H

#include "kernel/fs/fat32/fat32_structs.h"

uint32_t fat32_read_fat_entry(fat32_fs_t* fs, uint32_t cluster);
bool     fat32_write_fat_entry(fat32_fs_t* fs, uint32_t cluster, uint32_t value);
uint32_t fat32_allocate_cluster(fat32_fs_t* fs);
bool     fat32_free_cluster_chain(fat32_fs_t* fs, uint32_t start_cluster);

#endif /* KERNEL_FAT32_WRITE_H */