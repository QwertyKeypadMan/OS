#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"

bool virtio_gpu_resource_create(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t width, uint32_t height) {
    virtio_gpu_resource_create_2d_t cmd = {0};
    virtio_gpu_ctrl_hdr_t resp = {0};

    cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd.resource_id = res_id;
    cmd.format = 1; /* B8G8R8A8 / R8G8B8A8 */
    cmd.width = width;
    cmd.height = height;

    virtio_gpu_queue_send_cmd(dev, &cmd, sizeof(cmd), &resp, sizeof(resp));
    virtio_gpu_log(VIRTIO_LOG_PREFIX_RESOURCE, "2D Resource Olusturuldu ID: %d (%dx%d)", res_id, width, height);
    return true;
}

bool virtio_gpu_resource_attach(virtio_gpu_device_t *dev, uint32_t res_id, void *buffer, uint32_t size) {
    struct {
        virtio_gpu_resource_attach_backing_t req;
        virtio_gpu_mem_entry_t entry;
    } __attribute__((packed)) pkg = {0};

    virtio_gpu_ctrl_hdr_t resp = {0};

    pkg.req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    pkg.req.resource_id = res_id;
    pkg.req.nr_entries = 1;
    pkg.entry.addr = (uint64_t)(uintptr_t)buffer;
    pkg.entry.length = size;

    virtio_gpu_queue_send_cmd(dev, &pkg, sizeof(pkg), &resp, sizeof(resp));
    virtio_gpu_log(VIRTIO_LOG_PREFIX_RESOURCE, "Backing Memory Baglandi -> ResID: %d | Adres: 0x%x", res_id, (uintptr_t)buffer);
    return true;
}

bool virtio_gpu_resource_transfer(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    virtio_gpu_transfer_to_host_2d_t cmd = {0};
    virtio_gpu_ctrl_hdr_t resp = {0};

    cmd.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    cmd.resource_id = res_id;
    cmd.r.x = x;
    cmd.r.y = y;
    cmd.r.width = w;
    cmd.r.height = h;
    cmd.offset = (y * dev->width + x) * sizeof(uint32_t);

    virtio_gpu_queue_send_cmd(dev, &cmd, sizeof(cmd), &resp, sizeof(resp));
    virtio_gpu_log(VIRTIO_LOG_PREFIX_RESOURCE, "Transfer to Host 2D -> ResID: %d [%d,%d %dx%d]", res_id, x, y, w, h);
    return true;
}

bool virtio_gpu_resource_flush(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    virtio_gpu_resource_flush_t cmd = {0};
    virtio_gpu_ctrl_hdr_t resp = {0};

    cmd.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd.resource_id = res_id;
    cmd.r.x = x;
    cmd.r.y = y;
    cmd.r.width = w;
    cmd.r.height = h;

    virtio_gpu_queue_send_cmd(dev, &cmd, sizeof(cmd), &resp, sizeof(resp));
    virtio_gpu_log(VIRTIO_LOG_PREFIX_RESOURCE, "Resource Flush (Ekrana Basildi) -> ResID: %d", res_id);
    return true;
}