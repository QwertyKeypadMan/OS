#include "virtio_gpu.h"
#include "virtio_gpu_debug.h"

bool virtio_gpu_queue_setup(virtio_gpu_device_t *dev, uint16_t queue_index, virtqueue_t *vq) {
    if (!dev || !vq || queue_index >= VIRTIO_GPU_MAX_QUEUES) return false;

    /* Descriptor tablosunu ve ring yapılarını sıfırla */
    for (int i = 0; i < VIRTIO_GPU_QUEUE_SIZE; i++) {
        vq->desc[i].addr = 0;
        vq->desc[i].len = 0;
        vq->desc[i].flags = 0;
        vq->desc[i].next = 0;
    }
    vq->avail.flags = 0;
    vq->avail.idx = 0;
    vq->used.flags = 0;
    vq->used.idx = 0;

    virtio_gpu_log(VIRTIO_LOG_PREFIX_VQ, "VirtQueue %d kuruldu. Fiziksel Adres: 0x%x", queue_index, (uintptr_t)vq);
    return true;
}

void virtio_gpu_queue_send_cmd(virtio_gpu_device_t *dev, void *cmd, uint32_t cmd_size, void *resp, uint32_t resp_size) {
    if (!dev || !dev->vq_control || !cmd || !resp) return;

    virtqueue_t *vq = dev->vq_control;

    /* Descriptor 0: Outgoing Komut */
    vq->desc[0].addr = (uint64_t)(uintptr_t)cmd;
    vq->desc[0].len = cmd_size;
    vq->desc[0].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[0].next = 1;

    /* Descriptor 1: Incoming Yanıt Buffer */
    vq->desc[1].addr = (uint64_t)(uintptr_t)resp;
    vq->desc[1].len = resp_size;
    vq->desc[1].flags = VIRTQ_DESC_F_WRITE; /* GPU buraya yazacak */
    vq->desc[1].next = 0;

    /* Avail Ring'e Descriptor 0 ekle */
    uint16_t avail_idx = vq->avail.idx % VIRTIO_GPU_QUEUE_SIZE;
    vq->avail.ring[avail_idx] = 0;
    vq->avail.idx++;

    virtio_gpu_log(VIRTIO_LOG_PREFIX_VQ, "Komut Kuyruğa Eklendi. Komut Tipi: 0x%x", ((virtio_gpu_ctrl_hdr_t*)cmd)->type);

    /* Cihaza Bildir (Notify) - Legacy IO veya MMIO Doorbell */
    uint16_t used_idx_before = vq->used.idx;

 if (!dev->is_mmio && dev->io_base) {
        /* PCI I/O Notify register adresi `io_base + 0x10` civar\xc4\xb1ndad\xc4\xb1r */
        *(volatile uint16_t*)(uintptr_t)(dev->io_base + 0x10) = VIRTIO_GPU_QUEUE_CONTROL;
    }

    uint32_t timeout = 20000000;
    while (vq->used.idx == used_idx_before && --timeout) {
        __asm__ __volatile__("pause");
    }
    if (timeout == 0) {
        virtio_gpu_log(VIRTIO_LOG_PREFIX_VQ, "UYARI: Komut yaniti zaman asimina ugradi!");
    }
}