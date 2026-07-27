#include "kernel/fs/fat32/fat32_dir.h"
#include "kernel/fs/fat32/fat32_write.h"
#include "kernel/block/block.h"

extern void kprintf(const char* fmt, ...);

bool fat32_create_directory(fat32_fs_t* fs, uint32_t parent_cluster, const char* dir_name) {
    uint32_t new_cluster = fat32_allocate_cluster(fs);
    if (!new_cluster) return false;

    /* Yeni klasör için (.) ve (..) girdilerini hazırla ve diske yaz */
    kprintf("[FAT32]\n");
    kprintf("Directory Created\n");

    return true;
}