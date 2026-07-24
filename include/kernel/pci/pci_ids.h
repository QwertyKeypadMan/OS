#ifndef KERNEL_PCI_IDS_H
#define KERNEL_PCI_IDS_H

#include <stdint.h>

/* PCI Class Codes */
#define PCI_CLASS_UNCLASSIFIED          0x00
#define PCI_CLASS_MASS_STORAGE          0x01
#define PCI_CLASS_NETWORK               0x02
#define PCI_CLASS_DISPLAY               0x03
#define PCI_CLASS_MULTIMEDIA            0x04
#define PCI_CLASS_MEMORY                0x05
#define PCI_CLASS_BRIDGE                0x06
#define PCI_CLASS_COMMUNICATION         0x07
#define PCI_CLASS_SYSTEM_PERIPHERAL     0x08
#define PCI_CLASS_INPUT_DEVICE          0x09
#define PCI_CLASS_SERIAL_BUS            0x0C
#define PCI_CLASS_WIRELESS              0x0D

/* Subclasses - Mass Storage */
#define PCI_SUBCLASS_STORAGE_IDE        0x01
#define PCI_SUBCLASS_STORAGE_SATA       0x06
#define PCI_SUBCLASS_STORAGE_NVME       0x08

/* Subclasses - Display */
#define PCI_SUBCLASS_DISPLAY_VGA        0x00

/* Subclasses - Serial Bus */
#define PCI_SUBCLASS_SERIAL_USB         0x03

/* Class Code metin karşılığını getiren yardımcı fonksiyon */
const char* pci_get_class_name(uint8_t class_code, uint8_t subclass);

#endif /* KERNEL_PCI_IDS_H */