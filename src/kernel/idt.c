#include "kernel/idt.h"
#include "kernel/io.h"
#include <stdint.h>

/* KRITIK: bu pragma olmadan GCC, __attribute__((interrupt)) fonksiyonlari
 * icin urettigi stack hizalama prologue'unda SSE/x87 komutlari (movaps
 * vb.) kullanmaya calisiyor. Interrupt handler icinde FPU/SSE durumu
 * kaydedilmedigi icin derleyici bunu reddediyor ("80387 instructions
 * aren't allowed..."). general-regs-only bu sorunu tamamen ortadan
 * kaldirir: derleyici SADECE genel amacli registerlari (eax, ebx, vs.)
 * kullanmaya zorlanir. */
#pragma GCC target("general-regs-only")

#define IDT_ENTRIES 256

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

#define IDT_COM1 0x3F8

static void idt_serial_write(const char *s) {
    while (*s) {
        if (*s == '\n') {
            while ((inb(IDT_COM1 + 5) & 0x20) == 0) { }
            outb(IDT_COM1, '\r');
        }
        while ((inb(IDT_COM1 + 5) & 0x20) == 0) { }
        outb(IDT_COM1, (uint8_t)*s++);
    }
}

static void idt_serial_hex(uint32_t v) {
    static const char digits[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 0; i < 8; i++) {
        buf[9 - i] = digits[v & 0xF];
        v >>= 4;
    }
    idt_serial_write(buf);
}

static void idt_set_gate(int n, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].offset_high = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].selector    = selector;
    idt[n].zero        = 0;
    idt[n].type_attr   = type_attr;
}

static const char *exception_name(int vector) {
    switch (vector) {
    case 0:  return "Divide Error";
    case 1:  return "Debug";
    case 2:  return "NMI";
    case 3:  return "Breakpoint";
    case 4:  return "Overflow";
    case 5:  return "Bound Range Exceeded";
    case 6:  return "Invalid Opcode";
    case 7:  return "Device Not Available (FPU yok)";
    case 8:  return "Double Fault";
    case 10: return "Invalid TSS";
    case 11: return "Segment Not Present";
    case 12: return "Stack Fault";
    case 13: return "General Protection Fault";
    case 14: return "Page Fault";
    case 16: return "x87 FP Error";
    case 17: return "Alignment Check";
    case 18: return "Machine Check";
    case 19: return "SIMD FP Error";
    default: return "Bilinmeyen Istisna";
    }
}

static void fault_panic(int vector, uint32_t error_code, uint32_t eip, int has_error_code) {
    idt_serial_write("\n\n[KayaOS PANIC] CPU istisnasi yakalandi!\n");
    idt_serial_write("Vector : ");
    idt_serial_hex((uint32_t)vector);
    idt_serial_write("  (");
    idt_serial_write(exception_name(vector));
    idt_serial_write(")\n");

    if (has_error_code) {
        idt_serial_write("Error code: ");
        idt_serial_hex(error_code);
        idt_serial_write("\n");
    }

    idt_serial_write("EIP    : ");
    idt_serial_hex(eip);
    idt_serial_write("\n");

    if (vector == 14) {
        uint32_t cr2;
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        idt_serial_write("CR2 (hatali adres): ");
        idt_serial_hex(cr2);
        idt_serial_write("\n");
    }

    idt_serial_write("Sistem HALT edildi (RESET ATILMADI) -- bu ciktiyi paylas.\n");

    __asm__ __volatile__("cli");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

struct interrupt_frame {
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
};

#define DEFINE_ISR_NOERR(vec) \
    __attribute__((interrupt)) \
    static void isr_##vec(struct interrupt_frame *frame) { \
        fault_panic(vec, 0, frame->eip, 0); \
    }

#define DEFINE_ISR_ERR(vec) \
    __attribute__((interrupt)) \
    static void isr_##vec(struct interrupt_frame *frame, unsigned int error_code) { \
        fault_panic(vec, error_code, frame->eip, 1); \
    }

DEFINE_ISR_NOERR(0)
DEFINE_ISR_NOERR(1)
DEFINE_ISR_NOERR(2)
DEFINE_ISR_NOERR(3)
DEFINE_ISR_NOERR(4)
DEFINE_ISR_NOERR(5)
DEFINE_ISR_NOERR(6)
DEFINE_ISR_NOERR(7)
DEFINE_ISR_ERR(8)
DEFINE_ISR_ERR(10)
DEFINE_ISR_ERR(11)
DEFINE_ISR_ERR(12)
DEFINE_ISR_ERR(13)
DEFINE_ISR_ERR(14)
DEFINE_ISR_NOERR(16)
DEFINE_ISR_ERR(17)
DEFINE_ISR_NOERR(18)
DEFINE_ISR_NOERR(19)

void idt_init(void) {
    idtp.limit = (uint16_t)(sizeof(idt) - 1);
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    #define SET(vec) idt_set_gate(vec, (uint32_t)isr_##vec, 0x08, 0x8E)
    SET(0);  SET(1);  SET(2);  SET(3);
    SET(4);  SET(5);  SET(6);  SET(7);
    SET(8);  SET(10); SET(11); SET(12);
    SET(13); SET(14); SET(16); SET(17);
    SET(18); SET(19);
    #undef SET

    __asm__ __volatile__("lidt (%0)" : : "r"(&idtp));
}