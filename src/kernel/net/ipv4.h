#ifndef IPV4_H
#define IPV4_H

#include <stdint.h>
#include <stddef.h>

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

/* IPv4 Başlığı (Tam 20 Bayt - Seçeneksiz) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  version_ihl;   // Versiyon (4 bit) + IHL (4 bit)
    uint8_t  tos;           // Type of Service
    uint16_t total_len;     // Toplam Uzunluk
    uint16_t id;            // Kimlik
    uint16_t flags_fragment;// Bayraklar + Parça Ofseti
    uint8_t  ttl;           // Yaşam Süresi (Time to Live)
    uint8_t  protocol;      // Üst Katman Protokolü (ICMP=1, TCP=6, UDP=17)
    uint16_t checksum;      // Başlık Doğrulama Kodu
    uint8_t  src_ip[4];     // Kaynak IP
    uint8_t  dest_ip[4];    // Hedef IP
} ipv4_hdr_t;
#pragma pack(pop)

void ipv4_init(void);
uint16_t net_checksum(const void* data, size_t len);
void ipv4_handle_packet(const uint8_t* payload, uint16_t len);
int ipv4_send_packet(const uint8_t dest_ip[4], uint8_t protocol, const uint8_t* payload, uint16_t payload_len);

#endif /* IPV4_H */