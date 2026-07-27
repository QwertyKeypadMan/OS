#ifndef KERNEL_FAT32_FILE_H
#define KERNEL_FAT32_FILE_H

#include "kernel/fs/fat32/fat32_structs.h"

int32_t fat32_file_read(fat32_fs_t* fs, uint32_t start_cluster, uint32_t offset, uint32_t size, uint8_t* buffer);
int32_t fat32_file_write(fat32_fs_t* fs, uint32_t* start_cluster, uint32_t offset, uint32_t size, const uint8_t* buffer);
bool    fat32_file_create(fat32_fs_t* fs, uint32_t parent_cluster, const char* filename);
bool    fat32_file_delete(fat32_fs_t* fs, uint32_t parent_cluster, const char* filename);

#endif /* KERNEL_FAT32_FILE_H */