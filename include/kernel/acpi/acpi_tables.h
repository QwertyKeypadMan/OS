#ifndef ACPI_TABLES_H
#define ACPI_TABLES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#pragma pack(push, 1)

// Generic Address Structure (GAS) - ACPI 2.0+
typedef struct {
    uint8_t  address_space_id; // 0 = System Memory, 1 = System I/O
    uint8_t  register_bit_width;
    uint8_t  register_bit_offset;
    uint8_t  access_size;
    uint64_t address;
} __attribute__((packed)) acpi_gas_t;

// RSDP (Root System Description Pointer) v1.0 / v2.0
typedef struct {
    char     signature[8];     // "RSD PTR "
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;         // 0 = ACPI 1.0, 2 = ACPI 2.0+
    uint32_t rsdt_address;
    
    // ACPI 2.0+ tarlaları
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t  extended_checksum;
    uint8_t  reserved[3];
} __attribute__((packed)) rsdp_descriptor_t;

// Standart ACPI SDT Header
typedef struct {
    char     signature[4];
    uint32_t length;
    uint8_t  revision;
    uint8_t  checksum;
    char     oem_id[6];
    char     oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

// FADT (Fixed ACPI Description Table)
typedef struct {
    acpi_header_t header;
    uint32_t firmware_ctrl;
    uint32_t dsdt;
    uint8_t  reserved;
    uint8_t  preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t  acpi_enable;
    uint8_t  acpi_disable;
    uint8_t  S4bios_req;
    uint8_t  pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t  pm1_event_length;
    uint8_t  pm1_control_length;
    uint8_t  pm2_control_length;
    uint8_t  pm_timer_length;
    uint8_t  gpe0_length;
    uint8_t  gpe1_length;
    uint8_t  gpe1_base;
    uint8_t  cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alarm;
    uint8_t  month_alarm;
    uint8_t  century;
    uint16_t boot_architecture_flags;
    uint8_t  reserved2;
    uint32_t flags;
    
    // Reset Register (ACPI 2.0+)
    acpi_gas_t reset_reg;
    uint8_t    reset_value;
    uint8_t    reserved3[3];
    
    // 64-bit Adresler
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    acpi_gas_t x_pm1a_event_block;
    acpi_gas_t x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block;
    acpi_gas_t x_pm1b_control_block;
} __attribute__((packed)) fadt_t;

#pragma pack(pop)

bool acpi_validate_checksum(void* ptr, size_t length);
rsdp_descriptor_t* acpi_find_rsdp(void);
fadt_t* acpi_parse_tables(rsdp_descriptor_t* rsdp);

#endif