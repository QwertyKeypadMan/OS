#ifndef KERNEL_FAT32_DIR_H
#define KERNEL_FAT32_DIR_H

#include "kernel/fs/fat32/fat32_structs.h"

bool fat32_create_directory(fat32_fs_t* fs, uint32_t parent_cluster, const char* dir_name);
bool fat32_remove_directory(fat32_fs_t* fs, uint32_t parent_cluster, const char* dir_name);

#endif /* KERNEL_FAT32_DIR_H */