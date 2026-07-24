#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Her bir GDT satırının işlemci tarafından beklenen donanımsal yapısı
struct gdt_entry_struct {
    uint16_t limit_low;           // Segment sınırının ilk 16 biti
    uint16_t base_low;            // Taban adresinin ilk 16 biti
    uint8_t  base_middle;         // Taban adresinin sonraki 8 biti
    uint8_t  access;              // Erişim hakları (Ring 0 mı, Ring 3 mü vb.)
    uint8_t  granularity;         // Sayfa boyutu ve limit yüksek bitleri
    uint8_t  base_high;           // Taban adresinin son 8 biti
} __attribute__((packed));

// İşlemcinin LGDT komutuyla beklediği 6 byte'lık özel işaretçi
struct gdt_ptr_struct {
    uint16_t limit;               // Tüm tablonun boyutu (byte cinsinden - 1)
    uint32_t base;                // Tablonun RAM'deki tam fiziksel adresi
} __attribute__((packed));

// İşlemcinin beklediği TSS formatı
struct tss_entry_struct {
    uint32_t prev_tss;
    uint32_t esp0;       // Kernel'a dönüşte kullanılacak yığın (stack) adresi
    uint32_t ss0;        // Kernel Data Segment (0x10)
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap_flag;
    uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;
typedef struct gdt_entry_struct gdt_entry_t;
typedef struct gdt_ptr_struct gdt_ptr_t;

// Prototipler
void set_kernel_stack(uint32_t stack);
void tss_set_kernel_stack(uint32_t esp0);
uint32_t gdt_get_syscall_stack_top(void);
void init_gdt(void);

#endif