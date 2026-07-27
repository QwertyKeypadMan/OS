#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"

/* VirtIO MMIO Register Offset'leri (VirtIO Spec v1.1) */
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_STATUS              0x070

#define VIRTIO_MMIO_MAGIC_EXPECTED      0x74726976 /* 'virt' ascii */

static inline uint32_t mmio_read32(uintptr_t addr) {
    return *(volatile uint32_t*)addr;
}

static inline void mmio_write32(uintptr_t addr, uint32_t value) {
    *(volatile uint32_t*)addr = value;
}

bool virtio_gpu_mmio_verify(virtio_gpu_device_t *dev) {
    if (!dev || !dev->mmio_base) return false;

    uint32_t magic = mmio_read32(dev->mmio_base + VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MMIO_MAGIC_EXPECTED) {
        virtio_gpu_log(VIRTIO_LOG_PREFIX_MMIO, "HATA: Geceriz Magic Value: 0x%x", magic);
        return false;
    }

    uint32_t version = mmio_read32(dev->mmio_base + VIRTIO_MMIO_VERSION);
    uint32_t dev_id = mmio_read32(dev->mmio_base + VIRTIO_MMIO_DEVICE_ID);

    virtio_gpu_log(VIRTIO_LOG_PREFIX_MMIO, "MMIO Dogrulandi | Versiyon: %d | Device ID: %d", version, dev_id);

    if (dev_id != 16) { /* VirtIO GPU Device ID MMIO için 16'dir */
        virtio_gpu_log(VIRTIO_LOG_PREFIX_MMIO, "HATA: Cihaz VirtIO GPU degil!");
        return false;
    }

    return true;
}

void virtio_gpu_mmio_set_status(virtio_gpu_device_t *dev, uint8_t status) {
    if (!dev) return;
    if (dev->is_mmio && dev->mmio_base) {
        mmio_write32(dev->mmio_base + VIRTIO_MMIO_STATUS, status);
    }
    virtio_gpu_log(VIRTIO_LOG_PREFIX_MMIO, "Aygit Durumu Guncellendi -> Status Bayragi: 0x%x", status);
}