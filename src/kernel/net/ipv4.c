#include "ipv4.h"
#include "ethernet.h"
#include "arp.h"
#include "virtio_net.h"

extern void kserial_write(const char* str);
extern void icmp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len);

static uint8_t g_my_ip[4] = {10, 0, 2, 15};
static uint16_t g_ip_id = 1;

/* Internet Checksum (RFC 1071) Hesaplayıcı */
uint16_t net_checksum(const void* data, size_t len) {
    uint32_t sum = 0;
    const uint16_t* ptr = (const uint16_t*)data;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len > 0) {
        sum += *(const uint8_t*)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

void ipv4_init(void) {
    kserial_write("[IPv4] IPv4 katmani ilklendirildi (10.0.2.15)\r\n");
}

void ipv4_handle_packet(const uint8_t* payload, uint16_t len) {
    if (len < sizeof(ipv4_hdr_t)) return;

    const ipv4_hdr_t* ip = (const ipv4_hdr_t*)payload;

    /* Sadece bizim IP'mize gelen paketleri kabul et */
    if (ip->dest_ip[0] != g_my_ip[0] || ip->dest_ip[1] != g_my_ip[1] ||
        ip->dest_ip[2] != g_my_ip[2] || ip->dest_ip[3] != g_my_ip[3]) {
        return;
    }

    uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
    const uint8_t* ip_payload = payload + ihl;
    uint16_t ip_payload_len = ntohs(ip->total_len) - ihl;

    /* Protokole Göre Üst Katmana Yönlendir */
    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            icmp_handle_packet(ip->src_ip, ip_payload, ip_payload_len);
            break;
        default:
            break;
    }
}

int ipv4_send_packet(const uint8_t dest_ip[4], uint8_t protocol, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buffer[sizeof(ipv4_hdr_t) + payload_len];
    ipv4_hdr_t* ip = (ipv4_hdr_t*)buffer;

    ip->version_ihl = 0x45; // IPv4, IHL = 5 (20 bayt)
    ip->tos = 0;
    ip->total_len = htons(sizeof(ipv4_hdr_t) + payload_len);
    ip->id = htons(g_ip_id++);
    ip->flags_fragment = htons(0x4000); // Don't Fragment (DF)
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;

    for (int i = 0; i < 4; i++) {
        ip->src_ip[i] = g_my_ip[i];
        ip->dest_ip[i] = dest_ip[i];
    }

    /* Checksum Hesapla */
    ip->checksum = net_checksum(ip, sizeof(ipv4_hdr_t));

    /* Payload Kopyala */
    uint8_t* dst_payload = buffer + sizeof(ipv4_hdr_t);
    for (uint16_t i = 0; i < payload_len; i++) {
        dst_payload[i] = payload[i];
    }

    /* QEMU varsayılan router MAC adresi (52:54:00:12:34:56 veya Broadcast) */
    uint8_t dest_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return ethernet_send_frame(dest_mac, ETH_TYPE_IPV4, buffer, sizeof(ipv4_hdr_t) + payload_len);
}