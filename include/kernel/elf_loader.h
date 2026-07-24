#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>

// Fonksiyon göstericisi (Function Pointer) tanımı
typedef int (*elf_entry_point_t)(int argc, char **argv);

// ELF yükleme fonksiyonunun bildirimi
uint32_t load_elf_from_ramfs(const char *filename);

#endif