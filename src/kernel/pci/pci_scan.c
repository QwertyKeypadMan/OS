#include "kernel/pci/pci.h"
#include "kernel/pci/pci_ids.h"


/* KayaOS Kernel Printf / Logging bildirimi */
extern void kprintf(const char *fmt, ...);

/* pci.c dosyasından kullanılan iç fonksiyonlar */
extern void pci_add_device(const pci_device_t *dev);

/* BAR Boyutu ve Tipini Çözümleme */
static void pci_parse_bar(pci_device_t *dev, uint8_t bar_index) {
    uint8_t offset = 0x10 + (bar_index * 4);
    uint32_t bar_low = pci_read_config32(dev->bus, dev->device, dev->function, offset);

    if (bar_low == 0 || bar_low == 0xFFFFFFFF) {
        dev->bars[bar_index].type = BAR_TYPE_NONE;
        return;
    }

    /* I/O veya MMIO Kontrolü */
    if (bar_low & 0x01) {
        /* I/O Space BAR */
        dev->bars[bar_index].type = BAR_TYPE_IO;
        dev->bars[bar_index].base_address = bar_low & 0xFFFFFFFC;
        dev->bars[bar_index].is_prefetchable = false;
        dev->bars[bar_index].is_64bit = false;

        /* Boyut Hesaplama */
        pci_write_config32(dev->bus, dev->device, dev->function, offset, 0xFFFFFFFF);
        uint32_t mask = pci_read_config32(dev->bus, dev->device, dev->function, offset);
        pci_write_config32(dev->bus, dev->device, dev->function, offset, bar_low);
        dev->bars[bar_index].size = ~(mask & 0xFFFFFFFC) + 1;
    } else {
        /* Memory Space BAR */
        dev->bars[bar_index].type = BAR_TYPE_MMIO;
        dev->bars[bar_index].is_prefetchable = (bar_low & 0x08) != 0;
        bool is_64bit = ((bar_low >> 1) & 0x03) == 0x02;
        dev->bars[bar_index].is_64bit = is_64bit;

        uint64_t base = bar_low & 0xFFFFFFF0;

        if (is_64bit && bar_index < 5) {
            uint32_t bar_high = pci_read_config32(dev->bus, dev->device, dev->function, offset + 4);
            base |= ((uint64_t)bar_high << 32);
        }

        dev->bars[bar_index].base_address = base;

        /* Boyut Hesaplama */
        pci_write_config32(dev->bus, dev->device, dev->function, offset, 0xFFFFFFFF);
        uint32_t mask = pci_read_config32(dev->bus, dev->device, dev->function, offset);
        pci_write_config32(dev->bus, dev->device, dev->function, offset, bar_low);
        dev->bars[bar_index].size = ~(mask & 0xFFFFFFF0) + 1;

        if (is_64bit) {
            /* 64-bit BAR 2 adet slot kaplar */
            bar_index++;
            dev->bars[bar_index].type = BAR_TYPE_NONE;
        }
    }
}

/* Tek Bir Fonksiyonu Taramak */
static void pci_scan_function(uint8_t bus, uint8_t dev_num, uint8_t func) {
    uint16_t vendor_id = pci_read_config16(bus, dev_num, func, 0x00);
    if (vendor_id == 0xFFFF) return; /* Cihaz Yok */

    pci_device_t device;
    device.bus      = bus;
    device.device   = dev_num;
    device.function = func;
    device.vendor_id = vendor_id;
    device.device_id = pci_read_config16(bus, dev_num, func, 0x02);

    device.revision_id = pci_read_config8(bus, dev_num, func, 0x08);
    device.prog_if     = pci_read_config8(bus, dev_num, func, 0x09);
    device.subclass    = pci_read_config8(bus, dev_num, func, 0x0A);
    device.class_code  = pci_read_config8(bus, dev_num, func, 0x0B);

    device.header_type    = pci_read_config8(bus, dev_num, func, 0x0E);
    device.interrupt_line = pci_read_config8(bus, dev_num, func, 0x3C);
    device.interrupt_pin  = pci_read_config8(bus, dev_num, func, 0x3D);

    /* BAR'ları Oku (Header Type 0x00 Standart Cihazlar için BAR 0..5) */
    if ((device.header_type & 0x7F) == 0x00) {
        for (uint8_t i = 0; i < 6; i++) {
            pci_parse_bar(&device, i);
            if (device.bars[i].is_64bit) i++;
        }
    }

    /* Bulunan Cihazı Listeye Ekle */
    pci_add_device(&device);

    /* Belirtilen Format Standartında Kernel Logu Bas */
    const char *class_str = pci_get_class_name(device.class_code, device.subclass);

    kprintf("[PCI] Bus %d Dev %d Func %d\n", bus, dev_num, func);
    kprintf("Vendor : %04X\n", device.vendor_id);
    kprintf("Device : %04X\n", device.device_id);
    kprintf("Class  : %s\n", class_str);
    kprintf("---------------------------------\n");
}

/* Tüm PCI Veri Yolunu Tarama */
void pci_scan(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint16_t vendor = pci_read_config16((uint8_t)bus, dev, 0, 0x00);
            if (vendor == 0xFFFF) continue;

            pci_scan_function((uint8_t)bus, dev, 0);

            /* Multi-Function Cihaz Kontrolü (Header Type Bit 7) */
            uint8_t header_type = pci_read_config8((uint8_t)bus, dev, 0, 0x0E);
            if (header_type & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    pci_scan_function((uint8_t)bus, dev, func);
                }
            }
        }
    }
}