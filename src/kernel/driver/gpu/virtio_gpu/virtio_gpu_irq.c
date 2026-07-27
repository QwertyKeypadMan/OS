#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"

void virtio_gpu_irq_handler(void *arg) {
    virtio_gpu_device_t *dev = (virtio_gpu_device_t*)arg;
    if (!dev) return;

    virtio_gpu_log(VIRTIO_LOG_PREFIX_IRQ, "VirtIO GPU Kesmesi (IRQ) Tetiklendi! Komut Tamamlandi.");
}