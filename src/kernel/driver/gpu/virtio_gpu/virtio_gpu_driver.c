
#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"
#include "kernel/driver/driver.h"
#include "kernel/driver/driver_registry.h"
#include "kernel/pci/pci.h"
#include <stdlib.h>   /* malloc */
#include <stdint.h>

/* ---- Legacy VirtIO-PCI (pre-1.0 / transitional) I/O port register offsetleri ----
 * NOT: Bu offsetler QEMU'nun varsayilan (legacy uyumlu / transitional)
 * "-device virtio-gpu-pci" cihazi icindir. "disable-legacy=on" ile acilirsa
 * bu surucu CALISMAZ (modern, capability-tabanli MMIO gerekir - ayri is). */
#define VIRTIO_PCI_HOST_FEATURES   0x00
#define VIRTIO_PCI_GUEST_FEATURES  0x04
#define VIRTIO_PCI_QUEUE_PFN       0x08
#define VIRTIO_PCI_QUEUE_SIZE      0x0C
#define VIRTIO_PCI_QUEUE_SELECT    0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY    0x10
#define VIRTIO_PCI_STATUS          0x12

/* io.h'daki fonksiyonlarla cakismamak icin kendi port I/O yardimcilarimiz */
static inline void v_outb(uint16_t port, uint8_t val) { __asm__ volatile ("outb %0, %1" :: "a"(val), "Nd"(port)); }
static inline uint16_t v_inw(uint16_t port) { uint16_t r; __asm__ volatile ("inw %1, %0" : "=a"(r) : "Nd"(port)); return r; }
static inline void v_outl(uint16_t port, uint32_t val) { __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port)); }

static virtio_gpu_device_t g_virtio_gpu_dev;
static virtqueue_t g_vq_control __attribute__((aligned(4096)));
static virtqueue_t g_vq_cursor  __attribute__((aligned(4096)));

virtio_gpu_device_t* virtio_gpu_get_device(void) {
    return g_virtio_gpu_dev.initialized ? &g_virtio_gpu_dev : NULL;
}

static void gpu_set_status(virtio_gpu_device_t *dev, uint8_t status) {
    if (!dev->is_mmio && dev->io_base) {
        v_outb((uint16_t)(dev->io_base + VIRTIO_PCI_STATUS), status);
    } else {
        virtio_gpu_mmio_set_status(dev, status);
    }
}

static bool gpu_setup_queue(virtio_gpu_device_t *dev, uint16_t queue_index, virtqueue_t *vq) {
    if (!virtio_gpu_queue_setup(dev, queue_index, vq)) return false;

    if (!dev->is_mmio && dev->io_base) {
        v_outl((uint16_t)(dev->io_base + VIRTIO_PCI_QUEUE_SELECT), queue_index);
        uint16_t qsize = v_inw((uint16_t)(dev->io_base + VIRTIO_PCI_QUEUE_SIZE));
        if (qsize == 0) {
            virtio_gpu_log(VIRTIO_LOG_PREFIX_VQ, "HATA: Queue %d cihazda mevcut degil (size=0)", queue_index);
            return false;
        }
        uint32_t pfn = (uint32_t)(((uintptr_t)vq) >> 12); /* 4KB sayfa numarasi */
        v_outl((uint16_t)(dev->io_base + VIRTIO_PCI_QUEUE_PFN), pfn);
    }
    return true;
}

/* ---- Driver Manager Adaptoru ---- */
static int virtio_gpu_driver_probe(pci_device_t *dev) {
    if (!dev) return -1;
    if (dev->vendor_id == VIRTIO_VENDOR_ID &&
        (dev->device_id == VIRTIO_GPU_DEVICE_ID || dev->device_id == VIRTIO_GPU_LEGACY_DEV_ID)) {
        return 0;
    }
    return -1;
}

static int virtio_gpu_driver_init(pci_device_t *dev) {
    if (!dev) return -1;

    virtio_gpu_device_t *gpu = &g_virtio_gpu_dev;
    for (size_t i = 0; i < sizeof(*gpu); i++) ((uint8_t*)gpu)[i] = 0;

    /* pci_get_bar() su an BAR verisini kopyalamiyor (ayri bir bug),
     * bu yuzden pci_scan'in doldurdugu dev->bars[]'a DOGRUDAN erisiyoruz */
    pci_bar_t bar0 = dev->bars[0];
    if (bar0.type == BAR_TYPE_IO) {
        gpu->io_base = (uint32_t)bar0.base_address;
        gpu->is_mmio = false;
    } else if (bar0.type == BAR_TYPE_MMIO) {
        gpu->mmio_base = (uintptr_t)bar0.base_address;
        gpu->is_mmio = true;
    } else {
        kprintf("[VirtioGPU] HATA: BAR0 gecersiz (ne I/O ne MMIO)\n");
        return -1;
    }
    gpu->irq = dev->interrupt_line;

    if (gpu->is_mmio && !virtio_gpu_mmio_verify(gpu)) {
        kprintf("[VirtioGPU] UYARI: MMIO transport dogrulanamadi. "
                "Bu surucu su an sadece legacy I/O-port virtio-gpu-pci'yi destekliyor "
                "(QEMU'da disable-legacy=on KULLANMAYIN).\n");
        return -1;
    }

    /* --- VirtIO cihaz baslatma protokolu (spec 2.1.1 / legacy) --- */
    gpu_set_status(gpu, 0); /* reset */
    gpu_set_status(gpu, VIRTIO_STATUS_ACKNOWLEDGE);
    gpu_set_status(gpu, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    if (!gpu->is_mmio && gpu->io_base) {
        v_outl((uint16_t)(gpu->io_base + VIRTIO_PCI_GUEST_FEATURES), 0); /* ozellik istemiyoruz */
    }

    gpu->vq_control = &g_vq_control;
    gpu->vq_cursor  = &g_vq_cursor;
    if (!gpu_setup_queue(gpu, VIRTIO_GPU_QUEUE_CONTROL, gpu->vq_control)) {
        kprintf("[VirtioGPU] HATA: Control queue kurulamadi\n");
        return -1;
    }
    if (!gpu_setup_queue(gpu, VIRTIO_GPU_QUEUE_CURSOR, gpu->vq_cursor)) {
        kprintf("[VirtioGPU] UYARI: Cursor queue kurulamadi (donanimsal imlec devre disi)\n");
    }

    gpu_set_status(gpu, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    virtio_gpu_display_fetch_info(gpu);
    if (gpu->width == 0 || gpu->height == 0) {
        gpu->width = 1024;
        gpu->height = 768;
    }

    size_t fb_size = (size_t)gpu->width * (size_t)gpu->height * 4u;
    gpu->framebuffer_host_mem = malloc(fb_size);
    if (!gpu->framebuffer_host_mem) {
        kprintf("[VirtioGPU] HATA: Framebuffer icin bellek ayrilamadi (%u byte)\n", (unsigned)fb_size);
        return -1;
    }
    for (size_t i = 0; i < fb_size; i++) ((uint8_t*)gpu->framebuffer_host_mem)[i] = 0;

    gpu->primary_resource_id = 1;
    gpu->active_scanout = 0;

    if (!virtio_gpu_resource_create(gpu, gpu->primary_resource_id, gpu->width, gpu->height)) {
        kprintf("[VirtioGPU] HATA: 2D resource olusturulamadi\n");
        return -1;
    }
    if (!virtio_gpu_resource_attach(gpu, gpu->primary_resource_id, gpu->framebuffer_host_mem, (uint32_t)fb_size)) {
        kprintf("[VirtioGPU] HATA: Backing memory baglanamadi\n");
        return -1;
    }
    if (!virtio_gpu_display_set_scanout(gpu, gpu->active_scanout, gpu->primary_resource_id, gpu->width, gpu->height)) {
        kprintf("[VirtioGPU] HATA: Scanout baglanamadi\n");
        return -1;
    }

    gpu->initialized = true;
    kprintf("[VirtioGPU] Basariyla baslatildi: %ux%u ResID=%u\n", gpu->width, gpu->height, gpu->primary_resource_id);
    return 0;
}

static driver_t g_virtio_gpu_driver = {
    .name                 = "VirtIO GPU Driver",
    .version              = "0.1.0",
    .author               = "KayaOS Team",
    .supported_vendor_id  = VIRTIO_VENDOR_ID,
    .supported_device_id  = PCI_ANY_ID,
    .supported_class_code = PCI_ANY_CLASS,
    .supported_subclass   = PCI_ANY_CLASS,
    .priority             = 250,
    .probe                = virtio_gpu_driver_probe,
    .init                 = virtio_gpu_driver_init,
    .shutdown             = NULL,
    .remove               = NULL
};

void virtio_gpu_driver_register(void) {
    driver_register(&g_virtio_gpu_driver);
}
