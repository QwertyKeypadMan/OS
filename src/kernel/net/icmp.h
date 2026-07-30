#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

#pragma pack(push, 1)
typedef struct {
    uint8_t  type;     // 8 = Request, 0 = Reply
    uint8_t  code;     // 0
    uint16_t checksum; // ICMP Checksum
    uint16_t id;       // Identifier
    uint16_t sequence; // Sequence Number
} icmp_hdr_t;
#pragma pack(pop)

void icmp_handle_packet(const uint8_t* src_ip, const uint8_t* payload, uint16_t len);

#endif /* ICMP_H */