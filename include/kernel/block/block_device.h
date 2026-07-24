#ifndef KERNEL_BLOCK_DEVICE_H
#define KERNEL_BLOCK_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define BLOCK_DEV_NAME_MAX 64

/* Blok Aygıt Türleri */
typedef enum {
    BLOCK_DEV_TYPE_UNKNOWN = 0,
    BLOCK_DEV_TYPE_AHCI_SATA,
    BLOCK_DEV_TYPE_NVME,
    BLOCK_DEV_TYPE_USB_MASS,
    BLOCK_DEV_TYPE_RAMDISK,
    BLOCK_DEV_TYPE_VIRTUAL_DISK,
    BLOCK_DEV_TYPE_SD_CARD
} block_device_type_t;

/* Blok Aygıt Bayrakları (Flags) */
#define BLOCK_DEV_FLAG_READONLY  (1 << 0)
#define BLOCK_DEV_FLAG_REMOVABLE (1 << 1)
#define BLOCK_DEV_FLAG_PRESENT   (1 << 2)

struct block_device;

/* Sürücü Operasyonları (Callback Yapısı) */
typedef struct block_driver_ops {
    bool (*read)(struct block_device* dev, uint64_t start_lba, uint32_t count, uint8_t* buffer);
    bool (*write)(struct block_device* dev, uint64_t start_lba, uint32_t count, const uint8_t* buffer);
    bool (*flush)(struct block_device* dev);
    bool (*identify)(struct block_device* dev);
    bool (*reset)(struct block_device* dev);
} block_driver_ops_t;

/* Blok Aygıt Yapısı */
typedef struct block_device {
    char name[BLOCK_DEV_NAME_MAX];
    uint32_t id;
    block_device_type_t type;
    
    uint32_t sector_size;
    uint64_t sector_count;
    uint64_t capacity_bytes;
    
    uint32_t flags;
    
    block_driver_ops_t* ops;
    void* private_data;       /* Sürücüye özel veri (Örn: AHCI port pointer'ı) */
} block_device_t;

#endif /* KERNEL_BLOCK_DEVICE_H */