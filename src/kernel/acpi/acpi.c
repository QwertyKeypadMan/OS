#include "kernel/acpi/acpi.h"
#include <stdio.h>
#include <stddef.h>

static rsdp_descriptor_t* g_rsdp = NULL;
static fadt_t* g_fadt = NULL;

bool acpi_enable(void) {
    if (!g_fadt) return false;

    // ACPI zaten etkinleştirilmiş mi kontrolü
    if (g_fadt->smi_command_port && g_fadt->acpi_enable) {
        outb((uint16_t)g_fadt->smi_command_port, g_fadt->acpi_enable);
        
        // SCI etkinleşme beklemesi
        for (int i = 0; i < 300; i++) {
            if (inw((uint16_t)g_fadt->pm1a_control_block) & 1) {
                return true;
            }
        }
    }
    return true; 
}

bool acpi_init(void) {
    printf("[ACPI]\n");
    printf("Searching...\n");

    g_rsdp = acpi_find_rsdp();
    if (!g_rsdp) {
        printf("RSDP Not Found!\n");
        return false;
    }
    printf("RSDP Found\n");

    g_fadt = acpi_parse_tables(g_rsdp);
    if (!g_fadt) {
        printf("FADT Not Found!\n");
        return false;
    }
    printf("RSDT Found\n");
    printf("FADT Found\n");

    if (acpi_enable()) {
        printf("SCI Enabled\n");
    }

    acpi_power_init(g_fadt);
    printf("ACPI Ready\n");

    return true;
}