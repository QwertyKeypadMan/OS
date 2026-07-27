#ifndef KERNEL_PCI_H
#define KERNEL_PCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_PCI_DEVICES 64

/* Configuration Space Portları */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* BAR Tipleri */
typedef enum {
    BAR_TYPE_NONE = 0,
    BAR_TYPE_MMIO,
    BAR_TYPE_IO
} pci_bar_type_t;

/* BAR (Base Address Register) Yapısı */
typedef struct {
    uint64_t base_address;
    uint32_t size;
    pci_bar_type_t type;
    bool is_prefetchable;
    bool is_64bit;
} pci_bar_t;

/* PCI Cihaz Yapısı (Tüm alanlar tam olarak tanımlandı) */
typedef struct pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  revision_id;
    
    /* pci_scan.c için gerekli eksik alanlar: */
    uint8_t  header_type;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
	uint8_t  driver;

    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;

    pci_bar_t bars[6];
} pci_device_t;

/* Düşük Seviye Configuration Space API */
uint8_t  pci_read_config8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t pci_read_config16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint32_t pci_read_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

void pci_write_config8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t val);
void pci_write_config16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val);
void pci_write_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);

/* PCI Yönetim API */
void pci_init(void);
void pci_scan(void);

int           pci_get_device_count(void);
pci_device_t* pci_get_device(int index);
pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t* pci_find_device_by_class(uint8_t class_code, uint8_t subclass);
bool          pci_get_bar(const pci_device_t *dev, uint8_t bar_index, pci_bar_t *out_bar);

#endif /* KERNEL_PCI_H */