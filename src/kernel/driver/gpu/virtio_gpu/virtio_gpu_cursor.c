#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t pos;
    uint32_t resource_id;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t padding;
} virtio_gpu_update_cursor_t;

void virtio_gpu_cursor_update(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t x, uint32_t y) {
    if (!dev) return;
    virtio_gpu_update_cursor_t cmd = {0};
    cmd.hdr.type = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    cmd.resource_id = res_id;
    cmd.pos.x = x;
    cmd.pos.y = y;

    virtio_gpu_log(VIRTIO_LOG_PREFIX_DISPLAY, "Donanımsal Cursor Guncellendi -> X:%d Y:%d", x, y);
}