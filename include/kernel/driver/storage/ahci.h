#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stdbool.h>
#include "ahci_structs.h"

/* KayaOS Driver Manager Entegrasyon Yapıları */
typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;
    uintptr_t abar_phys;
} ahci_pci_device_t;

/* Public API */
void ahci_init(void);
bool ahci_probe(ahci_pci_device_t* pci_dev);
void ahci_scan_ports(void);
/* Driver Manager Kaydı İçin */
void ahci_driver_register(void);

/* Disk IO API */
ahci_port_device_t* ahci_find_port(uint8_t port_no);
bool ahci_identify(uint8_t port_no);
bool ahci_read(uint8_t port_no, uint64_t start_lba, uint32_t count, uint8_t* buffer);
bool ahci_write(uint8_t port_no, uint64_t start_lba, uint32_t count, const uint8_t* buffer);
bool ahci_reset_port(uint8_t port_no);

#endif /* AHCI_H */