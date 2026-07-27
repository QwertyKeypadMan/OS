#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"

bool virtio_gpu_display_fetch_info(virtio_gpu_device_t *dev) {
    virtio_gpu_ctrl_hdr_t cmd = {0};
    virtio_gpu_resp_display_info_t resp = {0};

    cmd.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    virtio_gpu_queue_send_cmd(dev, &cmd, sizeof(cmd), &resp, sizeof(resp));

    if (resp.pmodes[0].enabled) {
        dev->width = resp.pmodes[0].r.width;
        dev->height = resp.pmodes[0].r.height;
        virtio_gpu_log(VIRTIO_LOG_PREFIX_DISPLAY, "Ekran Bilgisi Alindi: %dx%d (Monitor 0 Active)", dev->width, dev->height);
    } else {
        /* Varsayılan Fallback Çözünürlüğü */
        dev->width = 1024;
        dev->height = 768;
        virtio_gpu_log(VIRTIO_LOG_PREFIX_DISPLAY, "Monitor Pasif. Varsayilan 1024x768 Atandi.");
    }

    return true;
}

bool virtio_gpu_display_set_scanout(virtio_gpu_device_t *dev, uint32_t scanout_id, uint32_t res_id, uint32_t w, uint32_t h) {
    virtio_gpu_set_scanout_t cmd = {0};
    virtio_gpu_ctrl_hdr_t resp = {0};

    cmd.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd.scanout_id = scanout_id;
    cmd.resource_id = res_id;
    cmd.r.x = 0;
    cmd.r.y = 0;
    cmd.r.width = w;
    cmd.r.height = h;

    virtio_gpu_queue_send_cmd(dev, &cmd, sizeof(cmd), &resp, sizeof(resp));
    virtio_gpu_log(VIRTIO_LOG_PREFIX_DISPLAY, "Scanout Baglandi: MonID %d -> ResID %d", scanout_id, res_id);
    return true;
}