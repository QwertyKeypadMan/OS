#ifndef SYS_SOCKET_H
#define SYS_SOCKET_H

#include <stdint.h>
#include <stddef.h>

#define AF_INET     2
#define SOCK_STREAM 1 // TCP
#define SOCK_DGRAM  2 // UDP

#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

struct in_addr {
    uint32_t s_addr;
};

struct sockaddr_in {
    uint16_t       sin_family;
    uint16_t       sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

/* BSD Socket API Fonksiyon Prototipleri */
int k_socket(int domain, int type, int protocol);
int k_connect(int sockfd, const struct sockaddr *addr, uint32_t addrlen);
int k_send(int sockfd, const void *buf, size_t len, int flags);
int k_recv(int sockfd, void *buf, size_t len, int flags);
int k_close_socket(int sockfd);

#endif /* SYS_SOCKET_H */