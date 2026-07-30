#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ETH_ALEN         6
#define ETH_HEADER_LEN   14

/* Sık kullanılan EtherType tanımları */
#define ETH_TYPE_IPV4    0x0800
#define ETH_TYPE_ARP     0x0806

/* Ethernet Başlık Yapısı (Tam 14 Bayt) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
} ethernet_hdr_t;
#pragma pack(pop)

/* Endianness (Bayt Sıralaması) Dönüştürücüleri */
static inline uint16_t htons(uint16_t val) {
    return (uint16_t)((val << 8) | (val >> 8));
}

static inline uint16_t ntohs(uint16_t val) {
    return htons(val);
}

/* Fonksiyon Protokolleri */
void ethernet_init(void);
int  ethernet_send_frame(const uint8_t dest_mac[6], uint16_t ethertype, const uint8_t* payload, uint16_t payload_len);
void ethernet_handle_packet(const uint8_t* frame, uint16_t len);
void ethernet_poll(void);

#endif /* ETHERNET_H */