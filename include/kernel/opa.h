#ifndef KERNEL_OPA_H
#define KERNEL_OPA_H

#include <stdbool.h>

/* cwd: RAMFS'te arama başlangıç dizini (genelde ramfs_root())
 * path: .opa dosyasının RAMFS yolu, örn: "/test.opa"
 * Basarili derleme + opa_main() bulundu ve calistirildiysa true doner. */
bool opa_run(int cwd, const char *path);

#endif