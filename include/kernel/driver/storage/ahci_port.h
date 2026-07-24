#ifndef AHCI_PORT_H
#define AHCI_PORT_H

#include <stdint.h>
#include <stdbool.h>
#include "ahci_structs.h"

/* Port Operasyon Arayüzleri */
ahci_device_type_t ahci_port_get_type(hba_port_t* port);
void               ahci_port_start(hba_port_t* port);
void               ahci_port_stop(hba_port_t* port);
bool               ahci_port_reset(hba_port_t* port);
void               ahci_port_rebase(ahci_port_device_t* dev, uint32_t port_no, uintptr_t base_phys_addr, uintptr_t base_virt_addr);

/* Disk I/O Operasyonları */
bool ahci_port_identify(ahci_port_device_t* dev);
bool ahci_port_read(ahci_port_device_t* dev, uint64_t start_lba, uint32_t count, uint8_t* buffer);
bool ahci_port_write(ahci_port_device_t* dev, uint64_t start_lba, uint32_t count, const uint8_t* buffer);

#endif /* AHCI_PORT_H */