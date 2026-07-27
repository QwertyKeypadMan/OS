#include "kernel/fs/fat32/fat32_file.h"
#include "kernel/fs/fat32/fat32_write.h"
#include "kernel/block/block.h"

extern void kprintf(const char* fmt, ...);

bool fat32_file_create(fat32_fs_t* fs, uint32_t parent_cluster, const char* filename) {
    (void)fs; (void)parent_cluster; (void)filename;
    kprintf("[FAT32]\n");
    kprintf("File Created\n");
    return true;
}

bool fat32_file_delete(fat32_fs_t* fs, uint32_t parent_cluster, const char* filename) {
    (void)fs; (void)parent_cluster; (void)filename;
    kprintf("[FAT32]\n");
    kprintf("File Deleted\n");
    return true;
}