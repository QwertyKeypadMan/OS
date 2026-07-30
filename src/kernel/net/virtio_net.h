#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* VirtIO Vendor & Device ID'leri */
#define VIRTIO_VENDOR_ID            0x1AF4
#define VIRTIO_NET_DEVICE_ID_LEGACY 0x1000
#define VIRTIO_NET_DEVICE_ID_MODERN 0x1041

/* VirtIO Status Register Bayrakları */
#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_FAILED        128

/* VirtIO Net Feature Bayrakları (SerenityOS spec) */
#define VIRTIO_NET_F_CSUM           (1 << 0)
#define VIRTIO_NET_F_GUEST_CSUM     (1 << 1)
#define VIRTIO_NET_F_MAC            (1 << 5)
#define VIRTIO_NET_F_STATUS         (1 << 16)
#define VIRTIO_NET_F_MRG_RXBUF      (1 << 15)

/* Queue İndeksleri */
#define VIRTIO_NET_QUEUE_RX         0
#define VIRTIO_NET_QUEUE_TX         1
#define VIRTIO_NET_QUEUE_CTRL       2

/* Paket & Buffer Sabitleri */
#define VIRTIO_NET_MAX_PACKET_SIZE  1514
#define VIRTIO_NET_QUEUE_SIZE       16

/* VirtIO Ring Descriptor Bayrakları */
#define VRING_DESC_F_NEXT           1
#define VRING_DESC_F_WRITE          2

/* VirtIO Network Paket Başlığı (virtio_net_hdr) */
#pragma pack(push, 1)
typedef struct {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} virtio_net_hdr_t;
#pragma pack(pop)

/* VirtIO Queue Descriptor Yapısı */
typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_NET_QUEUE_SIZE];
} __attribute__((packed)) vring_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) vring_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[VIRTIO_NET_QUEUE_SIZE];
} __attribute__((packed)) vring_used_t;

/* VirtIO Queue Sürücü Yapısı */
typedef struct {
    vring_desc_t* desc;
    vring_avail_t* avail;
    vring_used_t*  used;
    uint16_t      num_descriptors;
    uint16_t      free_head;
    uint16_t      last_seen_used;
} virtio_net_queue_t;

/* VirtIO Net Cihaz Durum Yapısı */
typedef struct {
    uint16_t io_base;
    uint8_t  mac[6];
    bool     link_up;
    bool     initialized;
    
    virtio_net_queue_t rx_queue;
    virtio_net_queue_t tx_queue;

    /* RX / TX Buffer Alanları */
    uint8_t rx_buffers[VIRTIO_NET_QUEUE_SIZE][sizeof(virtio_net_hdr_t) + VIRTIO_NET_MAX_PACKET_SIZE];
    uint8_t tx_buffers[VIRTIO_NET_QUEUE_SIZE][sizeof(virtio_net_hdr_t) + VIRTIO_NET_MAX_PACKET_SIZE];
} virtio_net_device_t;

/* Sürücü Fonksiyon Protokolleri */
int  virtio_net_init(uint16_t io_base);
int  virtio_net_send_packet(const uint8_t* data, uint16_t len);
int  virtio_net_receive_packet(uint8_t* buffer_out, uint32_t max_len);
void virtio_net_get_mac(uint8_t mac_out[6]);
void virtio_net_handle_irq(void);

#endif /* VIRTIO_NET_H */