#ifndef KERNEL_FAT32_MOUNT_H
#define KERNEL_FAT32_MOUNT_H

#include "kernel/fs/fat32/fat32_structs.h"

fat32_fs_t* fat32_mount(uint32_t block_dev_id);

#endif /* KERNEL_FAT32_MOUNT_H */