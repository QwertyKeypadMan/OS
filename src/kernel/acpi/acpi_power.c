#include "kernel/acpi/acpi_power.h"
#include "kernel/io.h"
#include <stddef.h>

static fadt_t* g_fadt = NULL;
static uint16_t SLP_TYPa = 0;
static uint16_t SLP_TYPb = 0;

void acpi_power_init(fadt_t* fadt_ptr) {
    g_fadt = fadt_ptr;
    
    // DSDT içerisindeki _S5 nesnesini ayrıştırma AML interpreter olmadan 
    // standart emülatörler/donanımlar için sabit/varsayılan değer kümesi:
    SLP_TYPa = (5 << 10); // Standard S5 value shift
    SLP_TYPb = (5 << 10);
}

void acpi_shutdown(void) {
    if (g_fadt && g_fadt->pm1a_control_block) {
        // ACPI Sleep Enable (SLP_EN) biti = 1 << 13
        uint16_t pm1a_s5 = SLP_TYPa | (1 << 13);
        outw((uint16_t)g_fadt->pm1a_control_block, pm1a_s5);

        if (g_fadt->pm1b_control_block) {
            uint16_t pm1b_s5 = SLP_TYPb | (1 << 13);
            outw((uint16_t)g_fadt->pm1b_control_block, pm1b_s5);
        }
    }

    // Fallback: QEMU / Bochs / VirtualBox kapatma portları
    outw(0x604, 0x2000);  // QEMU
    outw(0xB004, 0x2000); // Bochs / Older QEMU
    outw(0x4004, 0x3400); // VirtualBox
}

void acpi_reboot(void) {
    // 1. ACPI 2.0+ Reset Register denemesi
    if (g_fadt && (g_fadt->flags & (1 << 10))) { // RESET_REG_SUPPORTED
        uint64_t addr = g_fadt->reset_reg.address;
        uint8_t val = g_fadt->reset_value;

        if (g_fadt->reset_reg.address_space_id == 1) { // System I/O
            outb((uint16_t)addr, val);
        }
    }

    // 2. Fallback: Standard 8042 PS/2 Controller Reset
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);

    // 3. Fallback: Triple Fault
    __asm__ volatile ("cli; lidt (%0); int3" : : "r"(0));
}