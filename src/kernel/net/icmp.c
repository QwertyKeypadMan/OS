#include "icmp.h"
#include "ipv4.h"

extern void kserial_write(const char* str);

void icmp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len) {
    if (len < sizeof(icmp_hdr_t)) return;

    const icmp_hdr_t* icmp_in = (const icmp_hdr_t*)payload;

    /* Ping İsteği (Echo Request - Type 8) Yakalandı mı? */
    if (icmp_in->type == ICMP_TYPE_ECHO_REQUEST) {
        kserial_write("[ICMP] Ping Istegi (Echo Request) Alindi! Cevap veriliyor...\r\n");

        uint8_t reply_buf[len];
        icmp_hdr_t* icmp_out = (icmp_hdr_t*)reply_buf;

        /* Gelen paket verisini (payload/timestamp) koruyarak kopyala */
        for (uint16_t i = 0; i < len; i++) {
            reply_buf[i] = payload[i];
        }

        /* Yanıt Tipi: Echo Reply (0) */
        icmp_out->type = ICMP_TYPE_ECHO_REPLY;
        icmp_out->code = 0;
        icmp_out->checksum = 0;

        /* Yeniden Checksum Hesapla */
        icmp_out->checksum = net_checksum(reply_buf, len);

        /* IPv4 Üzerinden Cevabı Fırlat */
        ipv4_send_packet(src_ip, IP_PROTO_ICMP, reply_buf, len);
        kserial_write("[ICMP] Ping Cevabi (Echo Reply) Gonderildi! 🏓\r\n");
    }
}