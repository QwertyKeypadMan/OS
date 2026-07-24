#include "kernel/pci/pci.h"

/* Port I/O Inline Assembly Yardımcıları */
static inline void pci_outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint32_t pci_inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* PCI Config Adres Hesaplama */
static inline uint32_t pci_make_address(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    return (uint32_t)((bus << 16) | 
                      (dev << 11) | 
                      (func << 8) | 
                      (offset & 0xFC) | 
                      ((uint32_t)0x80000000));
}

/* 32-Bit Okuma */
uint32_t pci_read_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t address = pci_make_address(bus, dev, func, offset);
    pci_outl(PCI_CONFIG_ADDRESS, address);
    return pci_inl(PCI_CONFIG_DATA);
}

/* 16-Bit Okuma */
uint16_t pci_read_config16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config32(bus, dev, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

/* 8-Bit Okuma */
uint8_t pci_read_config8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config32(bus, dev, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

/* 32-Bit Yazma */
void pci_write_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    uint32_t address = pci_make_address(bus, dev, func, offset);
    pci_outl(PCI_CONFIG_ADDRESS, address);
    pci_outl(PCI_CONFIG_DATA, val);
}

/* 16-Bit Yazma */
void pci_write_config16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val) {
    uint32_t current = pci_read_config32(bus, dev, func, offset);
    uint32_t shift = (offset & 2) * 8;
    uint32_t mask = ~(0xFFFF << shift);
    uint32_t new_val = (current & mask) | (((uint32_t)val & 0xFFFF) << shift);
    pci_write_config32(bus, dev, func, offset, new_val);
}

/* 8-Bit Yazma */
void pci_write_config8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t val) {
    uint32_t current = pci_read_config32(bus, dev, func, offset);
    uint32_t shift = (offset & 3) * 8;
    uint32_t mask = ~(0xFF << shift);
    uint32_t new_val = (current & mask) | (((uint32_t)val & 0xFF) << shift);
    pci_write_config32(bus, dev, func, offset, new_val);
}