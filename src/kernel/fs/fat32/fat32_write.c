#include "kernel/fs/fat32/fat32_write.h"
#include "kernel/block/block.h"

extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

uint32_t fat32_read_fat_entry(fat32_fs_t* fs, uint32_t cluster) {
    block_device_t* dev = block_get_by_id(fs->block_dev_id);
    if (!dev) return FAT32_BAD_CLUSTER;

    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs->bpb.bytes_per_sector;

    uint8_t sector[512];
    if (!block_read(dev, fat_sector, 1, sector)) return FAT32_BAD_CLUSTER;

    uint32_t value = *(uint32_t*)&sector[entry_offset];
    return value & 0x0FFFFFFF; /* High 4 bit maskelenir */
}

bool fat32_write_fat_entry(fat32_fs_t* fs, uint32_t cluster, uint32_t value) {
    block_device_t* dev = block_get_by_id(fs->block_dev_id);
    if (!dev) return false;

    uint32_t fat_offset = cluster * 4;
    uint32_t entry_offset = fat_offset % fs->bpb.bytes_per_sector;

    /* Her iki FAT tablosunu da güncelle (FAT Mirroring) */
    for (uint8_t i = 0; i < fs->bpb.num_fats; i++) {
        uint32_t fat_sector = fs->fat_start_lba + (i * fs->bpb.fat_size_32) + (fat_offset / fs->bpb.bytes_per_sector);
        
        uint8_t sector[512];
        if (!block_read(dev, fat_sector, 1, sector)) return false;

        uint32_t current = *(uint32_t*)&sector[entry_offset];
        value = (value & 0x0FFFFFFF) | (current & 0xF0000000);
        *(uint32_t*)&sector[entry_offset] = value;

        if (!block_write(dev, fat_sector, 1, sector)) return false;
    }

    return true;
}

uint32_t fat32_allocate_cluster(fat32_fs_t* fs) {
    /* Basit Boş Küme Arama */
    uint32_t total_clusters = fs->bpb.total_sectors_32 / fs->sectors_per_cluster;
    for (uint32_t c = 2; c < total_clusters; c++) {
        if (fat32_read_fat_entry(fs, c) == FAT32_FREE_CLUSTER) {
            fat32_write_fat_entry(fs, c, 0x0FFFFFFF); /* EOF olarak işaretle */
            return c;
        }
    }
    return 0; /* Disk Dolu */
}

bool fat32_free_cluster_chain(fat32_fs_t* fs, uint32_t start_cluster) {
    uint32_t current = start_cluster;
    while (current >= 2 && current < 0x0FFFFFF7) {
        uint32_t next = fat32_read_fat_entry(fs, current);
        fat32_write_fat_entry(fs, current, FAT32_FREE_CLUSTER);
        current = next;
    }
    return true;
}