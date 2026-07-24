#include "kernel/gdt.h"
#include <stddef.h>

// Toplam 6 segmentimiz var: null, kernel code, kernel data, user code, user data, TSS
gdt_entry_t gdt_entries[6];
gdt_ptr_t   gdt_ptr;

// Assembly içindeki yükleyici fonksiyonlarımızı çağıracağız
extern void gdt_flush(uint32_t);
extern void tss_flush(void);

// GDT satırlarını kolayca doldurmak için yardımcı fonksiyon
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

// NOT: tss_entry_t tanımı gdt.h'den çekildiği için buradaki redundant struct tanımı kaldırıldı.
static tss_entry_t kernel_tss;

/* Ring3'ten kesme/syscall geldiginde CPU'nun otomatik gecis yapacagi
 * kernel stack'i (esp0). Su an tek bir process destekledigimiz icin
 * sabit bir stack yeterli. */
#define KERNEL_SYSCALL_STACK_SIZE 16384
static uint8_t kernel_syscall_stack[KERNEL_SYSCALL_STACK_SIZE] __attribute__((aligned(16)));

static void write_tss(int num, uint16_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&kernel_tss;
    uint32_t limit = base + sizeof(tss_entry_t);

    gdt_set_gate(num, base, limit, 0xE9, 0x00);

    for (size_t i = 0; i < sizeof(tss_entry_t); i++) {
        ((uint8_t *)&kernel_tss)[i] = 0;
    }

    kernel_tss.ss0 = ss0;
    kernel_tss.esp0 = esp0;
    kernel_tss.cs = 0x0B;
    kernel_tss.ss = kernel_tss.ds = kernel_tss.es = kernel_tss.fs = kernel_tss.gs = 0x13;
}

/* Bir process'e ring3'te calisma hakki verilmeden HEMEN once cagir --
 * boylece o process kesintiye ugrarsa CPU dogru kernel stack'ine doner. */
void tss_set_kernel_stack(uint32_t esp0) {
    kernel_tss.esp0 = esp0;
}

/* elf_loader.c gibi disaridan kernel syscall stack'inin tepesine
 * erismek isteyenler icin. */
uint32_t gdt_get_syscall_stack_top(void) {
    return (uint32_t)(kernel_syscall_stack + KERNEL_SYSCALL_STACK_SIZE);
}

void init_gdt() {
    // Tablonun limitini (boyutunu) ve RAM'deki adresini ayarla -- ARTIK 6 GIRIS
    gdt_ptr.limit = (sizeof(gdt_entry_t) * 6) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    // 1. İndis 0: Null Descriptor (İşlemci güvenliği için her zaman boş olmak zorundadır)
    gdt_set_gate(0, 0, 0, 0, 0);

    // 2. İndis 1: Kernel Code Segment (0x08 adresi) -> Ring 0 tam yetki
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 3. İndis 2: Kernel Data Segment (0x10 adresi) -> Ring 0 tam yetki
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    // 4. İndis 3: User Code Segment (0x1B adresi) -> Ring 3 Kullanıcı Modu
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    // 5. İndis 4: User Data Segment (0x23 adresi) -> Ring 3 Kullanıcı Modu
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // 6. İndis 5: TSS -- Ring3/Ring0 gecisleri icin sart (esp0 = kernel_syscall_stack tepesi)
    write_tss(5, 0x10, gdt_get_syscall_stack_top());

    // Yazdığımız bu tabloyu işlemciye yükle (Aşağıdaki assembly fonksiyonunu tetikler)
    gdt_flush((uint32_t)&gdt_ptr);

    // TR (Task Register) yukle -- TSS'i CPU'ya aktif olarak bildir
    tss_flush();
}