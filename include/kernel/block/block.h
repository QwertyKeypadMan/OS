#ifndef KERNEL_BLOCK_H
#define KERNEL_BLOCK_H

#include "kernel/block/block_device.h"

#define MAX_BLOCK_DEVICES 32

/* Core API */
void block_init(void);
int  block_register(block_device_t* dev);
bool block_unregister(uint32_t dev_id);

/* Search & Get API */
block_device_t* block_get_by_id(uint32_t dev_id);
block_device_t* block_find_by_name(const char* name);
block_device_t* block_get_primary(void);

/* I/O API */
bool block_read(block_device_t* dev, uint64_t start_lba, uint32_t count, uint8_t* buffer);
bool block_write(block_device_t* dev, uint64_t start_lba, uint32_t count, const uint8_t* buffer);
bool block_flush(block_device_t* dev);

/* Info API */
uint32_t block_get_sector_size(const block_device_t* dev);
uint64_t block_get_sector_count(const block_device_t* dev);
uint64_t block_get_capacity(const block_device_t* dev);

#endif /* KERNEL_BLOCK_H */