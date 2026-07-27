#include "kernel/fs/fat32/fat32_mount.h"
#include "kernel/block/block.h"

extern void kprintf(const char* fmt, ...);
extern void* kmalloc(size_t size);

fat32_fs_t* fat32_mount(uint32_t block_dev_id) {
    block_device_t* dev = block_get_by_id(block_dev_id);
    if (!dev) return NULL;

    fat32_fs_t* fs = (fat32_fs_t*)kmalloc(sizeof(fat32_fs_t));
    if (!fs) return NULL;

    fs->block_dev_id = block_dev_id;

    uint8_t sector_buf[512];
    if (!block_read(dev, 0, 1, sector_buf)) {
        kprintf("[FAT32] Error: Boot sector read failed!\n");
        return NULL;
    }

    /* BPB Kopyala */
    fat32_bpb_t* bpb = (fat32_bpb_t*)sector_buf;
    fs->bpb = *bpb;

    fs->fat_start_lba       = fs->bpb.reserved_sector_count;
    fs->cluster_heap_lba    = fs->bpb.reserved_sector_count + (fs->bpb.num_fats * fs->bpb.fat_size_32);
    fs->sectors_per_cluster = fs->bpb.sectors_per_cluster;
    fs->bytes_per_cluster   = fs->bpb.sectors_per_cluster * fs->bpb.bytes_per_sector;
    fs->root_cluster        = fs->bpb.root_cluster;

    kprintf("[FAT32]\n");
    kprintf("Mount Successful\n");
    
    return fs;
}