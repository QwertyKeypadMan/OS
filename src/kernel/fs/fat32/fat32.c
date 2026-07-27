#include "kernel/fs/fat32/fat32.h"
#include "kernel/fs/fat32/fat32_mount.h"
#include "kernel/fs/fat32/fat32_file.h"
#include "kernel/fs/fat32/fat32_dir.h"

extern void kprintf(const char* fmt, ...);

void fat32_init(void) {
    kprintf("[FAT32]\n");
    kprintf("Initializing Full Read-Write FAT32 Driver...\n");
}