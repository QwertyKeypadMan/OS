#ifndef ARP_H
#define ARP_H

#include <stdint.h>
#include <stdbool.h>

#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_PTYPE_IPV4     0x0800

#define ARP_OP_REQUEST     0x0001
#define ARP_OP_REPLY       0x0002

/* ARP Başlık Yapısı (Tam 28 Bayt) */
#pragma pack(push, 1)
typedef struct {
    uint16_t htype;         // Donanım Tipi (Ethernet = 1)
    uint16_t ptype;         // Protokol Tipi (IPv4 = 0x0800)
    uint8_t  hlen;          // Donanım Adres Uzunluğu (6 Bayt MAC)
    uint8_t  plen;          // Protokol Adres Uzunluğu (4 Bayt IP)
    uint16_t opcode;        // İşlem Kodu (1 = Request, 2 = Reply)
    uint8_t  sender_mac[6]; // Gönderen MAC
    uint8_t  sender_ip[4];  // Gönderen IP
    uint8_t  target_mac[6]; // Hedef MAC
    uint8_t  target_ip[4];  // Hedef IP
} arp_hdr_t;
#pragma pack(pop)

void arp_init(void);
void arp_handle_packet(const uint8_t* payload, uint16_t len);

#endif /* ARP_H */