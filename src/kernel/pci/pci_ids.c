#include "kernel/pci/pci_ids.h"

const char* pci_get_class_name(uint8_t class_code, uint8_t subclass) {
    switch (class_code) {
        case PCI_CLASS_UNCLASSIFIED:
            return "Unclassified Device";

        case PCI_CLASS_MASS_STORAGE:
            switch (subclass) {
                case PCI_SUBCLASS_STORAGE_IDE:  return "IDE Controller";
                case PCI_SUBCLASS_STORAGE_SATA: return "SATA Controller (AHCI)";
                case PCI_SUBCLASS_STORAGE_NVME: return "NVMe Controller";
                default:                        return "Mass Storage Controller";
            }

        case PCI_CLASS_NETWORK:
            return "Network Controller";

        case PCI_CLASS_DISPLAY:
            if (subclass == PCI_SUBCLASS_DISPLAY_VGA) {
                return "VGA Controller";
            }
            return "Display Controller";

        case PCI_CLASS_MULTIMEDIA:
            return "Audio Device";

        case PCI_CLASS_MEMORY:
            return "Memory Controller";

        case PCI_CLASS_BRIDGE:
            if (subclass == 0x00) return "Host Bridge";
            if (subclass == 0x01) return "ISA Bridge";
            if (subclass == 0x04) return "PCI Bridge";
            return "Bridge Device";

        case PCI_CLASS_COMMUNICATION:
            return "Serial/Communication Controller";

        case PCI_CLASS_SYSTEM_PERIPHERAL:
            return "System Peripheral";

        case PCI_CLASS_INPUT_DEVICE:
            return "Input Controller";

        case PCI_CLASS_SERIAL_BUS:
            if (subclass == PCI_SUBCLASS_SERIAL_USB) {
                return "USB Controller";
            }
            return "Serial Bus Controller";

        case PCI_CLASS_WIRELESS:
            return "Wireless Controller";

        default:
            return "Unknown Device";
    }
}