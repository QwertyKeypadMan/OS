#ifndef _KAYAOS_SYS_MMAN_H
#define _KAYAOS_SYS_MMAN_H

#include <stddef.h>

/* KayaOS'ta gercek sayfa koruma (W^X) sistemi yok -- linker ciktisi zaten
 * "LOAD segment has RWX permissions" uyarisi veriyor, yani her sayfa
 * zaten okunabilir+yazilabilir+calistirilabilir. Bu yuzden mmap/mprotect
 * burada sadece TCC'nin JIT bellek istegini kmalloc'a yonlendiren, gercek
 * bir izin/haritalama yapmayan minimal bir kopru. */

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED    0x01
#define MAP_PRIVATE   0x02
#define MAP_FIXED     0x10
#define MAP_ANONYMOUS 0x20
#define MAP_ANON      MAP_ANONYMOUS

#define MAP_FAILED ((void *)-1)

void *mmap(void *addr, size_t length, int prot, int flags, int fd, long offset);
int munmap(void *addr, size_t length);
int mprotect(void *addr, size_t len, int prot);

#endif /* _KAYAOS_SYS_MMAN_H */