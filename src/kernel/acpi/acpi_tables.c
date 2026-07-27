#include "kernel/acpi/acpi_tables.h"
#include <string.h>
#include <stddef.h>

// Temel bellek tarama aralığı (EBDA ve Main BIOS Area)
#define EBDA_SEARCH_START 0x00080000
#define EBDA_SEARCH_END   0x000A0000
#define BIOS_SEARCH_START 0x000E0000
#define BIOS_SEARCH_END   0x00100000

bool acpi_validate_checksum(void* ptr, size_t length) {
    uint8_t sum = 0;
    uint8_t* byte_ptr = (uint8_t*)ptr;
    for (size_t i = 0; i < length; i++) {
        sum += byte_ptr[i];
    }
    return (sum == 0);
}

rsdp_descriptor_t* acpi_find_rsdp(void) {
    // 1. EBDA Taraması
    for (uint32_t addr = EBDA_SEARCH_START; addr < EBDA_SEARCH_END; addr += 16) {
        if (memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            if (acpi_validate_checksum((void*)addr, sizeof(rsdp_descriptor_t))) {
                return (rsdp_descriptor_t*)addr;
            }
        }
    }
    
    // 2. BIOS Read-Only Area Taraması
    for (uint32_t addr = BIOS_SEARCH_START; addr < BIOS_SEARCH_END; addr += 16) {
        if (memcmp((void*)addr, "RSD PTR ", 8) == 0) {
            rsdp_descriptor_t* rsdp = (rsdp_descriptor_t*)addr;
            size_t len = (rsdp->revision >= 2) ? rsdp->length : 20;
            if (acpi_validate_checksum((void*)addr, len)) {
                return rsdp;
            }
        }
    }
    return NULL;
}

fadt_t* acpi_parse_tables(rsdp_descriptor_t* rsdp) {
    if (!rsdp) return NULL;

    acpi_header_t* root_sdt = NULL;
    bool is_xsdt = false;

    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        root_sdt = (acpi_header_t*)(uintptr_t)rsdp->xsdt_address;
        is_xsdt = true;
    } else {
        root_sdt = (acpi_header_t*)(uintptr_t)rsdp->rsdt_address;
    }

    if (!root_sdt || !acpi_validate_checksum(root_sdt, root_sdt->length)) {
        return NULL;
    }

    size_t entries = (root_sdt->length - sizeof(acpi_header_t)) / (is_xsdt ? 8 : 4);

    for (size_t i = 0; i < entries; i++) {
        acpi_header_t* header = NULL;
        if (is_xsdt) {
            uint64_t* ptrs = (uint64_t*)((uintptr_t)root_sdt + sizeof(acpi_header_t));
            header = (acpi_header_t*)(uintptr_t)ptrs[i];
        } else {
            uint32_t* ptrs = (uint32_t*)((uintptr_t)root_sdt + sizeof(acpi_header_t));
            header = (acpi_header_t*)(uintptr_t)ptrs[i];
        }

        if (header && memcmp(header->signature, "FACP", 4) == 0) {
            if (acpi_validate_checksum(header, header->length)) {
                return (fadt_t*)header;
            }
        }
    }

    return NULL;
}