#ifndef UDP_H
#define UDP_H

#include <stdint.h>

/* UDP Başlık Yapısı (Tam 8 Bayt) */
#pragma pack(push, 1)
typedef struct {
    uint16_t src_port;  // Kaynak Port
    uint16_t dest_port; // Hedef Port
    uint16_t length;    // Başlık + Veri Uzunluğu
    uint16_t checksum;  // Doğrulama Kodu (İsteğe Bağlı / 0x0000 yapılabilir)
} udp_hdr_t;
#pragma pack(pop)

void udp_init(void);
void udp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len);
int udp_send_packet(const uint8_t dest_ip[4], uint16_t src_port, uint16_t dest_port, const uint8_t* data, uint16_t data_len);

#endif /* UDP_H */