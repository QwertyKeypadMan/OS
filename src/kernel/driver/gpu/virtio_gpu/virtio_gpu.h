#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* VirtIO PCI & MMIO Tanımları */
#define VIRTIO_VENDOR_ID          0x1AF4
#define VIRTIO_GPU_DEVICE_ID      0x1050
#define VIRTIO_GPU_LEGACY_DEV_ID  0x1000

/* VirtIO Status Register Bayrakları */
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED     128

/* VirtQueue Sayıları */
#define VIRTIO_GPU_QUEUE_CONTROL  0
#define VIRTIO_GPU_QUEUE_CURSOR   1
#define VIRTIO_GPU_MAX_QUEUES     2
#define VIRTIO_GPU_QUEUE_SIZE     16

/* VirtIO GPU Komut Tipleri (Control Queue) */
typedef enum {
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
    VIRTIO_GPU_CMD_RESOURCE_UNREF,
    VIRTIO_GPU_CMD_SET_SCANOUT,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
    VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,

    /* Cursor Komutları */
    VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR,

    /* Yanıt Tipleri */
    VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO,
    VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
    VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY
} virtio_gpu_ctrl_type_t;

/* VirtQueue Descriptor Yapısı */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

#define VIRTQ_DESC_F_NEXT     1
#define VIRTQ_DESC_F_WRITE    2
#define VIRTQ_DESC_F_INDIRECT 4

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_GPU_QUEUE_SIZE];
} virtq_avail_t;

typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTIO_GPU_QUEUE_SIZE];
} virtq_used_t;

typedef struct {
    virtq_desc_t  desc[VIRTIO_GPU_QUEUE_SIZE];
    virtq_avail_t avail;
    uint8_t       padding[4096 - (sizeof(virtq_desc_t)*VIRTIO_GPU_QUEUE_SIZE + sizeof(virtq_avail_t))];
    virtq_used_t  used;
} __attribute__((aligned(4096))) virtqueue_t;

/* VirtIO GPU Komut Başlığı */
typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
} virtio_gpu_ctrl_hdr_t;

/* Geometry Structs */
typedef struct __attribute__((packed)) {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} virtio_gpu_rect_t;

/* Display Info Yapıları */
#define VIRTIO_GPU_MAX_SCANOUTS 16
typedef struct __attribute__((packed)) {
    virtio_gpu_rect_t r;
    uint32_t enabled;
    uint32_t flags;
} virtio_gpu_display_one_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_display_one_t pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} virtio_gpu_resp_display_info_t;

/* 2D Resource Komut Yapıları */
typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t format; /* VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1 */
    uint32_t width;
    uint32_t height;
} virtio_gpu_resource_create_2d_t;

typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} virtio_gpu_mem_entry_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
} virtio_gpu_resource_attach_backing_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_transfer_to_host_2d_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t resource_id;
    uint32_t padding;
} virtio_gpu_resource_flush_t;

typedef struct __attribute__((packed)) {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;
    uint32_t scanout_id;
    uint32_t resource_id;
} virtio_gpu_set_scanout_t;

/* VirtIO GPU Aygıt Bağlamı (Device Context) */
typedef struct {
    uint32_t io_base;
    uintptr_t mmio_base;
    uint8_t irq;
    bool is_mmio;
    
    /* VirtQueues */
    virtqueue_t *vq_control;
    virtqueue_t *vq_cursor;
    uint16_t last_used_ctrl_idx;

    /* Ekran ve Kaynak Detayları */
    uint32_t active_scanout;
    uint32_t width;
    uint32_t height;
    uint32_t primary_resource_id;
    void *framebuffer_host_mem;

    bool initialized;
} virtio_gpu_device_t;

/* Sürücü Fonksiyon Prototipleri */
bool virtio_gpu_mmio_verify(virtio_gpu_device_t *dev);
void virtio_gpu_mmio_set_status(virtio_gpu_device_t *dev, uint8_t status);

bool virtio_gpu_queue_setup(virtio_gpu_device_t *dev, uint16_t queue_index, virtqueue_t *vq);
void virtio_gpu_queue_send_cmd(virtio_gpu_device_t *dev, void *cmd, uint32_t cmd_size, void *resp, uint32_t resp_size);

bool virtio_gpu_resource_create(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t width, uint32_t height);
bool virtio_gpu_resource_attach(virtio_gpu_device_t *dev, uint32_t res_id, void *buffer, uint32_t size);
bool virtio_gpu_resource_transfer(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
bool virtio_gpu_resource_flush(virtio_gpu_device_t *dev, uint32_t res_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

bool virtio_gpu_display_fetch_info(virtio_gpu_device_t *dev);
bool virtio_gpu_display_set_scanout(virtio_gpu_device_t *dev, uint32_t scanout_id, uint32_t res_id, uint32_t w, uint32_t h);

void virtio_gpu_irq_handler(void *arg);
virtio_gpu_device_t* virtio_gpu_get_device(void);

#endif