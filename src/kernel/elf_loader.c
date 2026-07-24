#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/ramfs.h"

// --- ELF32 Yapıları ---
#define ELF_MAGIC 0x464C457F // "\x7FELF"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;     // Program Başlangıç Adresi
    uint32_t e_phoff;     // Program Header Tablosu Offset
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;     // Program Header Sayısı
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_header_t;

typedef struct {
    uint32_t p_type;   // 1 = PT_LOAD
    uint32_t p_offset; // Dosyadaki konumu
    uint32_t p_vaddr;  // Yükleneceği Sanal Adres
    uint32_t p_paddr;
    uint32_t p_filesz; // Dosyadaki boyutu
    uint32_t p_memsz;  // Bellekteki boyutu (BSS dahil)
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_ph_t;

#define PT_LOAD 1

// --- ELF Yükleyici Fonksiyonu ---
typedef int (*elf_entry_point_t)(int argc, char **argv);

uint32_t load_elf_from_ramfs(const char *filename) {
    int node_id = ramfs_resolve(0, filename);
    if (node_id < 0) {
        // Dosya bulunamadı
        return 0; 
    }

    const ramfs_node_t *node = ramfs_get(node_id);
    if (!node || node->type != RAMFS_NODE_FILE) {
        return 0;
    }

    const uint8_t *file_data = (const uint8_t *)node->data;
    elf32_header_t *header = (elf32_header_t *)file_data;

    // 1. Magic Number Kontrolü
    if (*(uint32_t *)header->e_ident != ELF_MAGIC) {
        // Geçersiz ELF dosyası
        return 0;
    }

    // 2. Program Segmentlerini (PT_LOAD) Belleğe Kopyala
    elf32_ph_t *ph = (elf32_ph_t *)(file_data + header->e_phoff);

    for (int i = 0; i < header->e_phnum; i++) {
        if (ph[i].p_type == PT_LOAD) {
            uint8_t *dest = (uint8_t *)ph[i].p_vaddr;
            const uint8_t *src = file_data + ph[i].p_offset;

            // Dosyadaki veriyi kopyala (.text, .data)
            for (size_t b = 0; b < ph[i].p_filesz; b++) {
                dest[b] = src[b];
            }

            // Geri kalan alanı (BSS) sıfırla
            for (size_t b = ph[i].p_filesz; b < ph[i].p_memsz; b++) {
                dest[b] = 0;
            }
        }
    }

    // Başlangıç adresini (entry point) döndür
    return header->e_entry;
}