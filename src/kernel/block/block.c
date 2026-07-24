#include "kernel/block/block.h"

extern void kprintf(const char* fmt, ...);

static block_device_t g_block_devices[MAX_BLOCK_DEVICES];
static uint32_t g_device_count = 0;

/* String kopyalama yardımcı fonksiyonu */
static void str_copy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] != '\0' && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* String karşılaştırma yardımcı fonksiyonu */
static int str_cmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void block_init(void) {
    kprintf("[BLOCK]\n");
    kprintf("Initializing...\n");

    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        g_block_devices[i].flags = 0;
        g_block_devices[i].id = 0;
        g_block_devices[i].private_data = NULL;
        g_block_devices[i].ops = NULL;
    }
    g_device_count = 0;
}

int block_register(block_device_t* dev) {
    if (!dev || !dev->ops) return -1;
    if (g_device_count >= MAX_BLOCK_DEVICES) {
        kprintf("[BLOCK] Error: Registry is full!\n");
        return -1;
    }

    uint32_t new_id = g_device_count + 1;
    dev->id = new_id;
    dev->flags |= BLOCK_DEV_FLAG_PRESENT;
    
    if (dev->sector_size > 0) {
        dev->capacity_bytes = dev->sector_count * dev->sector_size;
    }

    g_block_devices[g_device_count] = *dev;
    g_device_count++;

    uint64_t cap_gb = dev->capacity_bytes / (1024ULL * 1024ULL * 1024ULL);
    uint64_t cap_mb = dev->capacity_bytes / (1024ULL * 1024ULL);

    kprintf("[BLOCK]\n");
    kprintf("Registered Device\n");
    kprintf("Name : %s\n", dev->name);
    kprintf("Sector Size : %d\n", dev->sector_size);
    kprintf("Sector Count : %d\n", (uint32_t)dev->sector_count);
    if (cap_gb > 0) {
        kprintf("Capacity : %d GB\n", (uint32_t)cap_gb);
    } else {
        kprintf("Capacity : %d MB\n", (uint32_t)cap_mb);
    }

    return (int)new_id;
}

bool block_unregister(uint32_t dev_id) {
    for (uint32_t i = 0; i < g_device_count; i++) {
        if (g_block_devices[i].id == dev_id) {
            g_block_devices[i].flags &= ~BLOCK_DEV_FLAG_PRESENT;
            return true;
        }
    }
    return false;
}

block_device_t* block_get_by_id(uint32_t dev_id) {
    for (uint32_t i = 0; i < g_device_count; i++) {
        if (g_block_devices[i].id == dev_id && (g_block_devices[i].flags & BLOCK_DEV_FLAG_PRESENT)) {
            return &g_block_devices[i];
        }
    }
    return NULL;
}

block_device_t* block_find_by_name(const char* name) {
    if (!name) return NULL;
    for (uint32_t i = 0; i < g_device_count; i++) {
        if ((g_block_devices[i].flags & BLOCK_DEV_FLAG_PRESENT) &&
            str_cmp(g_block_devices[i].name, name) == 0) {
            return &g_block_devices[i];
        }
    }
    return NULL;
}

block_device_t* block_get_primary(void) {
    if (g_device_count > 0 && (g_block_devices[0].flags & BLOCK_DEV_FLAG_PRESENT)) {
        return &g_block_devices[0];
    }
    return NULL;
}

bool block_read(block_device_t* dev, uint64_t start_lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->ops || !dev->ops->read || !buffer) return false;
    return dev->ops->read(dev, start_lba, count, buffer);
}

bool block_write(block_device_t* dev, uint64_t start_lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !dev->ops || !dev->ops->write || !buffer) return false;
    if (dev->flags & BLOCK_DEV_FLAG_READONLY) return false;
    return dev->ops->write(dev, start_lba, count, buffer);
}

bool block_flush(block_device_t* dev) {
    if (!dev || !dev->ops) return false;
    if (dev->ops->flush) {
        return dev->ops->flush(dev);
    }
    return true;
}

uint32_t block_get_sector_size(const block_device_t* dev) {
    return dev ? dev->sector_size : 0;
}

uint64_t block_get_sector_count(const block_device_t* dev) {
    return dev ? dev->sector_count : 0;
}

uint64_t block_get_capacity(const block_device_t* dev) {
    return dev ? dev->capacity_bytes : 0;
}