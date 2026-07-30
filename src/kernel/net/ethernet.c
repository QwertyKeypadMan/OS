#include "ethernet.h"
#include "virtio_net.h"
#include "arp.h"
#include "ipv4.h"

/* Seri porta yazdırma fonksiyonu prototipi */
extern void kserial_write(const char* str);

static uint8_t g_our_mac[6];

/* Seri port için Hex yardımcıları */
static void serial_print_hex_byte(uint8_t byte) {
    const char hex_chars[] = "0123456789ABCDEF";
    char str[3];
    str[0] = hex_chars[(byte >> 4) & 0x0F];
    str[1] = hex_chars[byte & 0x0F];
    str[2] = '\0';
    kserial_write(str);
}

static void serial_print_mac(const uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        serial_print_hex_byte(mac[i]);
        if (i < 5) kserial_write(":");
    }
}

/* 1. Ethernet Katmanını Başlatma */
void ethernet_init(void) {
    virtio_net_get_mac(g_our_mac);
    
    kserial_write("[ETH] Ethernet katmani ilklendirildi. MAC: ");
    serial_print_mac(g_our_mac);
    kserial_write("\r\n");
}

/* 2. Ethernet Çerçevesi (Frame) Gönderme */
int ethernet_send_frame(const uint8_t dest_mac[6], uint16_t ethertype, const uint8_t* payload, uint16_t payload_len) {
    uint8_t frame_buffer[sizeof(ethernet_hdr_t) + payload_len];
    ethernet_hdr_t* hdr = (ethernet_hdr_t*)frame_buffer;

    /* Ethernet Başlığını Doldur */
    for (int i = 0; i < 6; i++) {
        hdr->dest_mac[i] = dest_mac[i];
        hdr->src_mac[i]  = g_our_mac[i];
    }
    hdr->ethertype = htons(ethertype); // Network Byte Order (Big-Endian)

    /* Paket Verisini (Payload) Başlığın Ardına Kopyala */
    uint8_t* payload_dst = frame_buffer + sizeof(ethernet_hdr_t);
    for (uint16_t i = 0; i < payload_len; i++) {
        payload_dst[i] = payload[i];
    }

    /* VirtIO Net Sürücüsüne Ver */
    return virtio_net_send_packet(frame_buffer, sizeof(ethernet_hdr_t) + payload_len);
}

/* 3. Gelen Paketi İşleme (Parsing) */
void ethernet_handle_packet(const uint8_t* frame, uint16_t len) {
    if (len < sizeof(ethernet_hdr_t)) {
        kserial_write("[ETH] Hata: Paket Ethernet basligindan kisa!\r\n");
        return;
    }

    const ethernet_hdr_t* hdr = (const ethernet_hdr_t*)frame;
    uint16_t ethertype = ntohs(hdr->ethertype);

   // kserial_write("[ETH] Paket Alindi | Kaynak MAC: ");
    serial_print_mac(hdr->src_mac);
   // kserial_write(" -> Type: 0x");
    serial_print_hex_byte((ethertype >> 8) & 0xFF);
    serial_print_hex_byte(ethertype & 0xFF);
   // kserial_write("\r\n");

    /* EtherType'a Göre Üst Katmanlara Yönlendir */
    switch (ethertype) {
        case ETH_TYPE_ARP:
            kserial_write("   └─> [ARP] Paket yakalandi! (Bir sonraki adimda işlenecek)\r\n");
            // arp_handle_packet(...)
			arp_handle_packet(frame + sizeof(ethernet_hdr_t), len - sizeof(ethernet_hdr_t));
            break;

        case ETH_TYPE_IPV4:
            kserial_write("   └─> [IPv4] Paket yakalandi!\r\n");
            // ipv4_handle_packet(...)
			ipv4_handle_packet(frame + sizeof(ethernet_hdr_t), len - sizeof(ethernet_hdr_t));
            break;

        default:
            //kserial_write("   └─> [Bilinmeyen Protokol]\r\n");
            break;
    }
}

/* 4. VirtIO Net'ten Paketleri Dinleme Döngüsü */
void ethernet_poll(void) {
    uint8_t rx_buffer[1514];
    int len = virtio_net_receive_packet(rx_buffer, sizeof(rx_buffer));

    if (len > 0) {
        ethernet_handle_packet(rx_buffer, (uint16_t)len);
    }
}