#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"
#include "kernel/pci/pci.h"

/* PCI Okuma Prototipi */
extern uint32_t pci_read_config32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

bool virtio_gpu_pci_probe(virtio_gpu_device_t *dev) {
    if (!dev) return false;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t vendor_device = pci_read_config32(bus, slot, 0, 0x00);
            uint16_t vendor = vendor_device & 0xFFFF;
            uint16_t device = (vendor_device >> 16) & 0xFFFF;

            if (vendor == VIRTIO_VENDOR_ID && (device == VIRTIO_GPU_DEVICE_ID || device == VIRTIO_GPU_LEGACY_DEV_ID)) {
                virtio_gpu_log(VIRTIO_LOG_PREFIX_MAIN, "PCI Cihaz Tespiti Başarılı! Bus:%d Slot:%d [Vendor:0x%x Device:0x%x]", bus, slot, vendor, device);

                uint32_t bar0 = pci_read_config32(bus, slot, 0, 0x10);
                if (bar0 & 1) {
                    dev->io_base = bar0 & ~0x3;
                    dev->is_mmio = false;
                    virtio_gpu_log(VIRTIO_LOG_PREFIX_MAIN, "I/O Port Base: 0x%x", dev->io_base);
                } else {
                    dev->mmio_base = bar0 & ~0xF;
                    dev->is_mmio = true;
                    virtio_gpu_log(VIRTIO_LOG_PREFIX_MAIN, "MMIO Address Base: 0x%x", dev->mmio_base);
                }

                dev->irq = (uint8_t)(pci_read_config32(bus, slot, 0, 0x3C) & 0xFF);
                return true;
            }
        }
    }

    virtio_gpu_log(VIRTIO_LOG_PREFIX_MAIN, "PCI Tarama Tamamlandi. VirtIO GPU Bulunamadi.");
    return false;
}