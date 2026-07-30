#include "tcp.h"
#include "ipv4.h"
#include "kernel/io.h"
#include "endian.h"

extern void kserial_write(const char* str);

void tcp_init(void) {
    kserial_write("[TCP] TCP katmani hazir.\r\n");
}

#include "socket.h"

/* socket.c içinde tanımlayacağımız soket veri alma fonksiyonu */
extern void socket_push_rx_data(uint16_t dest_port, const uint8_t* data, uint16_t len);

void tcp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len) {
    (void)src_ip;
    if (len < sizeof(tcp_hdr_t)) return;

    const tcp_hdr_t* tcp_in = (const tcp_hdr_t*)payload;

    uint16_t src_port  = ntohs(tcp_in->src_port);
    uint16_t dest_port = ntohs(tcp_in->dest_port);
    uint32_t seq       = ntohl(tcp_in->seq_num);
    uint32_t ack       = ntohl(tcp_in->ack_num);
    uint8_t  flags     = tcp_in->flags;

    uint8_t header_len = (tcp_in->data_offset >> 4) * 4;
    const uint8_t* tcp_data = payload + header_len;
    uint16_t data_len = len - header_len;

    /* 1. SYN Geldiyse Handshake Yap */
    if (flags & TCP_SYN) {
        tcp_hdr_t tcp_out;
        tcp_out.src_port    = htons(dest_port);
        tcp_out.dest_port   = htons(src_port);
        tcp_out.seq_num     = htonl(1000);
        tcp_out.ack_num     = htonl(seq + 1);
        tcp_out.data_offset = (5 << 4);
        tcp_out.flags       = TCP_SYN | TCP_ACK;
        tcp_out.window_size = htons(8192);
        tcp_out.checksum    = 0;
        tcp_out.urgent_ptr  = 0;

        ipv4_send_packet(src_ip, IP_PROTO_TCP, (const uint8_t*)&tcp_out, sizeof(tcp_hdr_t));
    }
    /* 2. Veri Geldiyse (PSH veya Normal Veri) -> Soket Tamponuna At */
    else if (data_len > 0) {
        socket_push_rx_data(dest_port, tcp_data, data_len);

        /* Veriyi Aldık ACK'si Gönder */
        tcp_hdr_t tcp_out;
        tcp_out.src_port    = htons(dest_port);
        tcp_out.dest_port   = htons(src_port);
        tcp_out.seq_num     = htonl(ack);
        tcp_out.ack_num     = htonl(seq + data_len);
        tcp_out.data_offset = (5 << 4);
        tcp_out.flags       = TCP_ACK;
        tcp_out.window_size = htons(8192);
        tcp_out.checksum    = 0;
        tcp_out.urgent_ptr  = 0;

        ipv4_send_packet(src_ip, IP_PROTO_TCP, (const uint8_t*)&tcp_out, sizeof(tcp_hdr_t));
    }
}