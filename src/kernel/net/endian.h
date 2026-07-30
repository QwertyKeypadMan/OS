#ifndef ENDIAN_H
#define ENDIAN_H

#include <stdint.h>

/*
 * x86 / x86_64 mimarisi Little-Endian kullanır.
 * Ağ protokolleri (TCP/IP) ise Big-Endian kullanır.
 * GCC built-in fonksiyonları ile işlemci seviyesinde donanımsal bayt takası yapıyoruz.
 */

#define htons(x) __builtin_bswap16((uint16_t)(x))
#define ntohs(x) __builtin_bswap16((uint16_t)(x))
#define htonl(x) __builtin_bswap32((uint32_t)(x))
#define ntohl(x) __builtin_bswap32((uint32_t)(x))

#endif /* ENDIAN_H */