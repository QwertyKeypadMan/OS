#include "arp.h"
#include "ethernet.h"
#include "virtio_net.h"

extern void kserial_write(const char* str);

/* QEMU varsayılan sanal ağ IP adresimiz */
static uint8_t g_my_ip[4] = {10, 0, 2, 15};

void arp_init(void) {
    kserial_write("[ARP] ARP katmani ilklendirildi (IP: 10.0.2.15)\r\n");
}

void arp_handle_packet(const uint8_t* payload, uint16_t len) {
    if (len < sizeof(arp_hdr_t)) {
        return;
    }

    const arp_hdr_t* arp = (const arp_hdr_t*)payload;

    uint16_t opcode = ntohs(arp->opcode);
    uint16_t htype  = ntohs(arp->htype);
    uint16_t ptype  = ntohs(arp->ptype);

    /* Sadece Ethernet ve IPv4 tabanlı ARP paketlerini işle */
    if (htype != ARP_HTYPE_ETHERNET || ptype != ARP_PTYPE_IPV4) {
        return;
    }

    /* 1. Birisi bizim IP'mizi sorguluyorsa (ARP Request) */
    if (opcode == ARP_OP_REQUEST) {
        if (arp->target_ip[0] == g_my_ip[0] &&
            arp->target_ip[1] == g_my_ip[1] &&
            arp->target_ip[2] == g_my_ip[2] &&
            arp->target_ip[3] == g_my_ip[3]) {

            kserial_write("[ARP] Bize ARP Sorgusu Geldi! Cevap (Reply) hazirlaniyor...\r\n");

            arp_hdr_t reply;
            reply.htype  = htons(ARP_HTYPE_ETHERNET);
            reply.ptype  = htons(ARP_PTYPE_IPV4);
            reply.hlen   = 6;
            reply.plen   = 4;
            reply.opcode = htons(ARP_OP_REPLY);

            /* Gönderen kısmına kendi MAC ve IP'mizi yazıyoruz */
            virtio_net_get_mac(reply.sender_mac);
            for (int i = 0; i < 4; i++) reply.sender_ip[i] = g_my_ip[i];

            /* Hedef kısmına sorguyu atan cihazın MAC ve IP'sini yazıyoruz */
            for (int i = 0; i < 6; i++) reply.target_mac[i] = arp->sender_mac[i];
            for (int i = 0; i < 4; i++) reply.target_ip[i] = arp->sender_ip[i];

            /* Ethernet üzerinden cevabı fırlat */
            ethernet_send_frame(arp->sender_mac, ETH_TYPE_ARP, (const uint8_t*)&reply, sizeof(arp_hdr_t));
            kserial_write("[ARP] ARP Yaniti (Reply) basariyla gonderildi!\r\n");
        }
    }
}