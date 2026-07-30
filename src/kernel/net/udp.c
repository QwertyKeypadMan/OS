#include "udp.h"
#include "ipv4.h"
#include "endian.h"

extern void kserial_write(const char* str);

void udp_init(void) {
    kserial_write("[UDP] UDP katmani hazir.\r\n");
}

void udp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len) {
    if (len < sizeof(udp_hdr_t)) return;

    const udp_hdr_t* udp = (const udp_hdr_t*)payload;

    uint16_t src_port  = ntohs(udp->src_port);
    uint16_t dest_port = ntohs(udp->dest_port);
    uint16_t udp_len   = ntohs(udp->length);

    const uint8_t* udp_data = payload + sizeof(udp_hdr_t);
    uint16_t data_len = udp_len - sizeof(udp_hdr_t);

    kserial_write("[UDP] Paket Alindi! ");
    // Basit bir Echo testi: 8080 portuna gelen veriyi ekrana bas ve geri fırlat (Echo Server)
    if (dest_port == 8080) {
        kserial_write("(Port 8080 - Echo Servisi)\r\n");
        udp_send_packet(src_ip, 8080, src_port, udp_data, data_len);
    } else {
        kserial_write("(Bilinmeyen Port)\r\n");
    }
}

int udp_send_packet(const uint8_t dest_ip[4], uint16_t src_port, uint16_t dest_port, const uint8_t* data, uint16_t data_len) {
    uint16_t total_len = sizeof(udp_hdr_t) + data_len;
    uint8_t buffer[total_len];

    udp_hdr_t* udp = (udp_hdr_t*)buffer;
    udp->src_port  = htons(src_port);
    udp->dest_port = htons(dest_port);
    udp->length    = htons(total_len);
    udp->checksum  = 0; // IPv4'te UDP checksum kullanımı opsiyoneldir (0 verilebilir)

    uint8_t* payload = buffer + sizeof(udp_hdr_t);
    for (uint16_t i = 0; i < data_len; i++) {
        payload[i] = data[i];
    }

    return ipv4_send_packet(dest_ip, IP_PROTO_UDP, buffer, total_len);
}