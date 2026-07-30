#ifndef TCP_H
#define TCP_H

#include <stdint.h>

/* TCP Bayrakları (Flags) */
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

/* TCP Başlık Yapısı (Tam 20 Bayt - Opsiyonsuz) */
#pragma pack(push, 1)
typedef struct {
    uint16_t src_port;     // Kaynak Port
    uint16_t dest_port;    // Hedef Port
    uint32_t seq_num;      // Sıra Numarası (Sequence Number)
    uint32_t ack_num;      // Onay Numarası (Acknowledgment Number)
    uint8_t  data_offset;  // Data Offset (4 bit) + Reserved (4 bit)
    uint8_t  flags;        // TCP Bayrakları
    uint16_t window_size;  // Pencere Boyutu
    uint16_t checksum;     // Checksum
    uint16_t urgent_ptr;   // Acil Durum İşaretçisi
} tcp_hdr_t;
#pragma pack(pop)

void tcp_init(void);
void tcp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len);

#endif /* TCP_H */