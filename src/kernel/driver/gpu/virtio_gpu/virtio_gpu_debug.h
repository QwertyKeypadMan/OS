#ifndef VIRTIO_GPU_DEBUG_H
#define VIRTIO_GPU_DEBUG_H

#include <stdarg.h>

/* Kernel kprintf/printk fonksiyonu çağrılır */
extern void kprintf(const char *fmt, ...);

#define VIRTIO_GPU_LOG_LEVEL_DEBUG 0
#define VIRTIO_GPU_LOG_LEVEL_INFO  1
#define VIRTIO_GPU_LOG_LEVEL_WARN  2
#define VIRTIO_GPU_LOG_LEVEL_ERROR 3

#define VIRTIO_LOG_PREFIX_MAIN     "[VIRTIO GPU]"
#define VIRTIO_LOG_PREFIX_VQ       "[VQ]"
#define VIRTIO_LOG_PREFIX_RESOURCE "[RESOURCE]"
#define VIRTIO_LOG_PREFIX_DISPLAY  "[DISPLAY]"
#define VIRTIO_LOG_PREFIX_IRQ      "[IRQ]"
#define VIRTIO_LOG_PREFIX_MMIO     "[MMIO]"

#define virtio_gpu_log(prefix, fmt, ...) \
    kprintf("%s " fmt "\n", prefix, ##__VA_ARGS__)

#endif