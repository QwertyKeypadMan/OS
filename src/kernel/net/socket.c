#include "socket.h"
#include "tcp.h"
#include "udp.h"
#include "ipv4.h"
#include "kernel/kstring.h"

#define MAX_SOCKETS 16

typedef enum {
    SOCKET_STATE_FREE,
    SOCKET_STATE_CREATED,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSED
} socket_state_t;

typedef struct {
    int id;
    int domain;
    int type;
    int protocol;
    socket_state_t state;
    uint8_t dest_ip[4];
    uint16_t dest_port;
    uint16_t src_port;
    
    /* Gelen veriler için dairesel ara bellek (Ring Buffer) */
    uint8_t rx_buffer[2048];
    uint16_t rx_head;
    uint16_t rx_tail;
} kaya_socket_t;

static kaya_socket_t g_sockets[MAX_SOCKETS];
static uint16_t g_next_ephemeral_port = 49152;

int k_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (g_sockets[i].state == SOCKET_STATE_FREE) {
            g_sockets[i].id = i + 1;
            g_sockets[i].domain = domain;
            g_sockets[i].type = type;
            g_sockets[i].protocol = protocol;
            g_sockets[i].state = SOCKET_STATE_CREATED;
            g_sockets[i].src_port = g_next_ephemeral_port++;
            g_sockets[i].rx_head = 0;
            g_sockets[i].rx_tail = 0;
            return g_sockets[i].id;
        }
    }
    return -1; // Soket sınırı aşıldı
}

#include <stdint.h>

/* --- Byte Order (Endianness) Dönüştürücüleri (x86 Little-Endian -> Network Big-Endian) --- */

static inline uint16_t htons(uint16_t v) {
    return (uint16_t)((v << 8) | (v >> 8));
}

static inline uint16_t ntohs(uint16_t v) {
    return htons(v);
}

static inline uint32_t htonl(uint32_t v) {
    return ((v & 0x000000FF) << 24) |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0xFF000000) >> 24);
}

static inline uint32_t ntohl(uint32_t v) {
    return htonl(v);
}

int k_connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen) {
    (void)addrlen;
    if (sockfd <= 0 || sockfd > MAX_SOCKETS) return -1;

    kaya_socket_t *sock = &g_sockets[sockfd - 1];
    if (sock->state != SOCKET_STATE_CREATED) return -1;

    const struct sockaddr_in *in_addr = (const struct sockaddr_in *)addr;
    
    /* IP ve Port bilgilerini kaydet */
    uint32_t ip = in_addr->sin_addr.s_addr;
    sock->dest_ip[0] = (ip >> 0) & 0xFF;
    sock->dest_ip[1] = (ip >> 8) & 0xFF;
    sock->dest_ip[2] = (ip >> 16) & 0xFF;
    sock->dest_ip[3] = (ip >> 24) & 0xFF;
    sock->dest_port  = ntohs(in_addr->sin_port);

    sock->state = SOCKET_STATE_CONNECTED;
    return 0;
}

int k_send(int sockfd, const void *buf, size_t len, int flags) {
    (void)flags;
    if (sockfd <= 0 || sockfd > MAX_SOCKETS) return -1;

    kaya_socket_t *sock = &g_sockets[sockfd - 1];
    if (sock->state != SOCKET_STATE_CONNECTED) return -1;

    if (sock->type == SOCK_STREAM) { // TCP
        /* TCP Başlığı Oluşturup Gönder */
        tcp_hdr_t tcp_out;
        tcp_out.src_port    = htons(sock->src_port);
        tcp_out.dest_port   = htons(sock->dest_port);
        tcp_out.seq_num     = htonl(100);
        tcp_out.ack_num     = htonl(0);
        tcp_out.data_offset = (5 << 4);
        tcp_out.flags       = TCP_PSH | TCP_ACK;
        tcp_out.window_size = htons(8192);
        tcp_out.checksum    = 0;
        tcp_out.urgent_ptr  = 0;

        /* Payload ile birleştirip fırlatıyoruz */
        uint8_t pkt[sizeof(tcp_hdr_t) + len];
        k_memcpy(pkt, &tcp_out, sizeof(tcp_hdr_t));
        k_memcpy(pkt + sizeof(tcp_hdr_t), buf, len);

        ipv4_send_packet(sock->dest_ip, IP_PROTO_TCP, pkt, sizeof(tcp_hdr_t) + len);
        return (int)len;
    } else if (sock->type == SOCK_DGRAM) { // UDP
        udp_send_packet(sock->dest_ip, sock->src_port, sock->dest_port, buf, len);
        return (int)len;
    }

    return -1;
}

int k_recv(int sockfd, void *buf, size_t len, int flags) {
    (void)flags;
    if (sockfd <= 0 || sockfd > MAX_SOCKETS) return -1;

    kaya_socket_t *sock = &g_sockets[sockfd - 1];
    if (sock->state != SOCKET_STATE_CONNECTED) return -1;

    /* Soket tamponunda veri var mı kontrol et */
    uint16_t available = sock->rx_head - sock->rx_tail;
    if (available == 0) return 0; // Veri bekleniyor (non-blocking)

    size_t to_read = (len < available) ? len : available;
    k_memcpy(buf, sock->rx_buffer + sock->rx_tail, to_read);
    sock->rx_tail += to_read;

    if (sock->rx_tail == sock->rx_head) {
        sock->rx_head = 0;
        sock->rx_tail = 0;
    }

    return (int)to_read;
}

int k_close_socket(int sockfd) {
    if (sockfd <= 0 || sockfd > MAX_SOCKETS) return -1;
    g_sockets[sockfd - 1].state = SOCKET_STATE_FREE;
    return 0;
}

/* TCP Katmanından gelen veriyi doğru porttaki soketin rx_buffer'ına doldurur */
void socket_push_rx_data(uint16_t dest_port, const uint8_t* data, uint16_t len) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (g_sockets[i].state == SOCKET_STATE_CONNECTED && g_sockets[i].src_port == dest_port) {
            for (uint16_t d = 0; d < len; d++) {
                if (g_sockets[i].rx_head < sizeof(g_sockets[i].rx_buffer)) {
                    g_sockets[i].rx_buffer[g_sockets[i].rx_head++] = data[d];
                }
            }
            break;
        }
    }
}