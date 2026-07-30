#include "virtio_net.h"
#include "kernel/io.h"
#include "kernel/kstring.h"

/* Global Cihaz Örneği */
static virtio_net_device_t g_virtio_net_dev;

/* Queue Hazırlama Fonksiyonu */
static void virtio_net_setup_queue(virtio_net_queue_t* q, uint16_t size) {
    k_memset(q, 0, sizeof(virtio_net_queue_t));
    q->num_descriptors = size;
    q->free_head = 0;
    q->last_seen_used = 0;
}

/* RX Queue'yu İstek Almaya Hazırlama */
static void virtio_net_populate_rx_buffers(void) {
    virtio_net_queue_t* rx_q = &g_virtio_net_dev.rx_queue;

    for (int i = 0; i < VIRTIO_NET_QUEUE_SIZE; i++) {
        vring_desc_t* desc = &rx_q->desc[i];
        desc->addr = (uint64_t)(uintptr_t)g_virtio_net_dev.rx_buffers[i];
        desc->len = sizeof(virtio_net_hdr_t) + VIRTIO_NET_MAX_PACKET_SIZE;
        desc->flags = VRING_DESC_F_WRITE; // Aygıt bu alana yazacak
        desc->next = 0;

        rx_q->avail->ring[rx_q->avail->idx % VIRTIO_NET_QUEUE_SIZE] = i;
        rx_q->avail->idx++;
    }

    // Aygıta yeni RX buffer'larının hazır olduğunu bildir
    outw(g_virtio_net_dev.io_base + 0x10, VIRTIO_NET_QUEUE_RX);
}

/* VirtIO Net Sürücü İlklendirmesi */
int virtio_net_init(uint16_t io_base) {
    k_memset(&g_virtio_net_dev, 0, sizeof(virtio_net_device_t));
    g_virtio_net_dev.io_base = io_base;

    /* 1. Device Reset */
    outb(io_base + 0x12, 0);

    /* 2. ACKNOWLEDGE & DRIVER durum biti koy */
    outb(io_base + 0x12, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* 3. Özellikleri oku & Negotiate et */
    uint32_t device_features = inl(io_base + 0x00);
    uint32_t driver_features = device_features & (VIRTIO_NET_F_MAC | VIRTIO_NET_F_CSUM | VIRTIO_NET_F_STATUS);
    outl(io_base + 0x04, driver_features);

    outb(io_base + 0x12, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    /* 4. MAC Adresini Cihaz Config Alanından Oku (Base Offset 0x14) */
    if (device_features & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < 6; i++) {
            g_virtio_net_dev.mac[i] = inb(io_base + 0x14 + i);
        }
    }

    /* 5. RX ve TX Kuyruklarını Kur */
    virtio_net_setup_queue(&g_virtio_net_dev.rx_queue, VIRTIO_NET_QUEUE_SIZE);
    virtio_net_setup_queue(&g_virtio_net_dev.tx_queue, VIRTIO_NET_QUEUE_SIZE);

    /* RX Buffers doldur */
    virtio_net_populate_rx_buffers();

    /* 6. DRIVER_OK ile cihazı aktifleştir */
    outb(io_base + 0x12, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);

    g_virtio_net_dev.initialized = true;
    g_virtio_net_dev.link_up = true;

    return 0;
}

/* Paket Gönderme Mantığı (VirtIO TX Queue) */
int virtio_net_send_packet(const uint8_t* data, uint16_t len) {
    if (!g_virtio_net_dev.initialized || len > VIRTIO_NET_MAX_PACKET_SIZE) {
        return -1;
    }

    virtio_net_queue_t* tx_q = &g_virtio_net_dev.tx_queue;
    uint16_t desc_idx = tx_q->free_head % VIRTIO_NET_QUEUE_SIZE;

    /* Header + Data paketini TX buffer'a kopyala */
    virtio_net_hdr_t* hdr = (virtio_net_hdr_t*)g_virtio_net_dev.tx_buffers[desc_idx];
    k_memset(hdr, 0, sizeof(virtio_net_hdr_t)); // Offload olmadan varsayılan sıfırlanmış header

    k_memcpy(g_virtio_net_dev.tx_buffers[desc_idx] + sizeof(virtio_net_hdr_t), data, len);

    /* VirtIO Descriptor Oluştur */
    vring_desc_t* desc = &tx_q->desc[desc_idx];
    desc->addr = (uint64_t)(uintptr_t)g_virtio_net_dev.tx_buffers[desc_idx];
    desc->len = sizeof(virtio_net_hdr_t) + len;
    desc->flags = 0; // Read-only by device
    desc->next = 0;

    /* Available Ring'e Ekle */
    tx_q->avail->ring[tx_q->avail->idx % VIRTIO_NET_QUEUE_SIZE] = desc_idx;
    tx_q->avail->idx++;
    tx_q->free_head++;

    /* VirtIO Aygıtını Uyar (Queue Notify - TX Queue 1) */
    outw(g_virtio_net_dev.io_base + 0x10, VIRTIO_NET_QUEUE_TX);

    return 0;
}

/* Paket Alma Mantığı (VirtIO RX Queue - Düzeltilmiş/Eksiksiz Sürüm) */
int virtio_net_receive_packet(uint8_t* out_buffer, uint32_t max_len) {
    if (!g_virtio_net_dev.initialized) {
        return 0;
    }

    virtio_net_queue_t* rx_q = &g_virtio_net_dev.rx_queue;

    /* 1. Cihaz tarafından işlenmiş yeni paket var mı kontrol et (Used Ring) */
    if (rx_q->last_seen_used == rx_q->used->idx) {
        return 0; // Yeni paket yok
    }

    /* 2. Used Ring'den sıradaki paketin descriptor indeksini ve uzunluğunu al */
    uint16_t used_idx = rx_q->last_seen_used % VIRTIO_NET_QUEUE_SIZE;
    vring_used_elem_t used_elem = rx_q->used->ring[used_idx];

    uint32_t desc_idx = used_elem.id;
    uint32_t total_len = used_elem.len;

    /* Yalnızca VirtIO header veya daha küçük geçersiz paket kontrolü */
    if (total_len <= sizeof(virtio_net_hdr_t)) {
        rx_q->last_seen_used++;
        return 0;
    }

    /* 3. Tampon adresi & VirtIO başlığını (10 bayt) atlama */
    uint8_t* raw_rx_data = g_virtio_net_dev.rx_buffers[desc_idx];
    uint8_t* eth_frame_start = raw_rx_data + sizeof(virtio_net_hdr_t);
    uint32_t eth_frame_len = total_len - sizeof(virtio_net_hdr_t);

    if (eth_frame_len > max_len) {
        eth_frame_len = max_len;
    }

    /* 4. Gerçek Ethernet paketini hedef değişkene kopyala */
    k_memcpy(out_buffer, eth_frame_start, eth_frame_len);

    /* 5. Kullanılan Descriptor'ı Avail Ring'e geri koy (Queue Refill) */
    rx_q->avail->ring[rx_q->avail->idx % VIRTIO_NET_QUEUE_SIZE] = desc_idx;
    rx_q->avail->idx++;
    rx_q->last_seen_used++;

    /* Cihaza yeni RX tampon alanının hazır olduğunu haber ver */
    outw(g_virtio_net_dev.io_base + 0x10, VIRTIO_NET_QUEUE_RX);

    return (int)eth_frame_len;
}

/* MAC Adresini Dışarı Veren Yardımcı Fonksiyon */
void virtio_net_get_mac(uint8_t mac_out[6]) {
    k_memcpy(mac_out, g_virtio_net_dev.mac, 6);
}

/* Kesme (IRQ) İşleyicisi */
void virtio_net_handle_irq(void) {
    if (!g_virtio_net_dev.initialized) return;

    /* ISR Status Oku & Temizle (Legacy VirtIO Offset 0x13) */
    uint8_t isr = inb(g_virtio_net_dev.io_base + 0x13);
    (void)isr;
}