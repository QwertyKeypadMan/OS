#include "kernel/driver/storage/ahci.h"
#include "kernel/driver/storage/ahci_port.h"
#include "kernel/driver/driver.h"
#include "kernel/driver/driver_registry.h"
#include "kernel/pci/pci.h"
#include "kernel/kstring.h"
#include "kernel/block/block.h"

extern void kprintf(const char* fmt, ...);

/* =========================================================================
 * AHCI DRIVER MANAGER ADAPTER FONKSİYONLARI
 * ========================================================================= */

/**
 * Driver Manager için Probe testi.
 * Sadece Class 0x01, Subclass 0x06 ve Programming Interface (prog_if) 0x01 (AHCI)
 * olan cihazları doğrular. IDE veya RAID modundaki diskleri eler.
 */
static int ahci_driver_probe(pci_device_t* dev) {
    if (!dev) return -1;

    /* Class: Storage (0x01), Subclass: SATA (0x06), Prog IF: AHCI (0x01) */
    if (dev->class_code == AHCI_PCI_CLASS_STORAGE &&
        dev->subclass   == AHCI_PCI_SUBCLASS_SATA &&
        dev->prog_if    == AHCI_PCI_PROG_IF_AHCI) 
    {
        return 0; /* Başarılı eşleşme */
    }

    return -1; /* AHCI değil (IDE veya RAID modu) */
}

/**
 * Driver Manager tarafından eşleşme sağlandığında çağrılan ilklendirme fonksiyonu.
 */
static int ahci_driver_init(pci_device_t* dev) {
    if (!dev) return -1;

    /* BAR5 (ABAR) bilgisini pci_get_bar yardımıyla çekiyoruz */
    pci_bar_t bar5;
    if (!pci_get_bar(dev, 5, &bar5)) {
        kprintf("[AHCI] Error: Could not get BAR5 (ABAR)!\n");
        return -1;
    }

    uintptr_t abar_phys = (uintptr_t)bar5.base_address;

    if (abar_phys == 0) {
        kprintf("[AHCI] Error: ABAR (BAR5) address is invalid!\n");
        return -1;
    }

    /* AHCI PCI cihaz bilgisini hazırla (func ve slot/device alanları düzeltildi) */
    ahci_pci_device_t ahci_dev = {
        .vendor_id = dev->vendor_id,
        .device_id = dev->device_id,
        .bus       = dev->bus,
        .slot = dev->device,   
		
	    .func = dev->function, 
         .abar_phys = abar_phys
    };

    /* AHCI Controller'ı başlat ve portları tara */
    if (!ahci_probe(&ahci_dev)) {
        kprintf("[AHCI] Error: Failed to initialize AHCI controller.\n");
        return -2;
    }

    return 0; /* Başarıyla ilklendirildi */
}

/* =========================================================================
 * AHCI DRIVER TANIMI (driver_t)
 * ========================================================================= */

static driver_t g_ahci_driver = {
    .name                 = "AHCI SATA Controller Driver",
    .version              = "1.0.0",
    .author               = "KayaOS Team",
    
    /* Donanım Eşleşme Filtresi */
    .supported_vendor_id  = PCI_ANY_ID,          /* Tüm markalar (Intel, AMD, QEMU vs.) */
    .supported_device_id  = PCI_ANY_ID,
    .supported_class_code = AHCI_PCI_CLASS_STORAGE, /* 0x01 */
    .supported_subclass   = AHCI_PCI_SUBCLASS_SATA,    /* 0x06 */
    
    .priority             = 200,                  /* Generic sürücülerden yüksek öncelik */
    
    /* Sürücü Yaşam Döngüsü */
    .probe                = ahci_driver_probe,
    .init                 = ahci_driver_init,
    .shutdown             = NULL,
    .remove               = NULL
};

/**
 * AHCI Sürücüsünü Driver Registry'ye kaydeden fonksiyon.
 * Sistem boot anında (kernel_main içinde pci_scan öncesi) çağrılır.
 */
void ahci_driver_register(void) {
    driver_register(&g_ahci_driver);
}

/* KayaOS Kernel Log Fonksiyonu Bildirimi */
extern void kprintf(const char* fmt, ...);

/* Dynamic DMA Buffer Allocator (Kernel MMU'ya uyarlanmalı) */
extern void* kmalloc_aligned(size_t size, size_t alignment);

static hba_mem_t*          g_abar = NULL;
static ahci_port_device_t  g_ports[AHCI_MAX_PORTS];

static const char* ahci_get_type_name(ahci_device_type_t type) {
    switch (type) {
        case AHCI_DEV_SATA:   return "SATA SSD";
        case AHCI_DEV_SATAPI: return "SATAPI";
        case AHCI_DEV_SEMB:   return "SEMB";
        case AHCI_DEV_PM:     return "Port Multiplier";
        default:              return "Empty";
    }
}

static bool ahci_block_read(block_device_t* dev, uint64_t start_lba, uint32_t count, uint8_t* buffer) {
    if (!dev || !dev->private_data) return false;
    ahci_port_device_t* port_dev = (ahci_port_device_t*)dev->private_data;
    return ahci_port_read(port_dev, start_lba, count, buffer);
}

static bool ahci_block_write(block_device_t* dev, uint64_t start_lba, uint32_t count, const uint8_t* buffer) {
    if (!dev || !dev->private_data) return false;
    ahci_port_device_t* port_dev = (ahci_port_device_t*)dev->private_data;
    return ahci_port_write(port_dev, start_lba, count, buffer);
}

static bool ahci_block_flush(block_device_t* dev) {
    (void)dev;
    return true; /* AHCI komut kuyruğu otomatik sync edilir */
}

static block_driver_ops_t g_ahci_block_ops = {
    .read     = ahci_block_read,
    .write    = ahci_block_write,
    .flush    = ahci_block_flush,
    .identify = NULL,
    .reset    = NULL
};

void ahci_scan_ports(void) {
    if (!g_abar) return;

    uint32_t pi = g_abar->pi;
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        if (pi & (1U << i)) {
            hba_port_t* port = &g_abar->ports[i];
            ahci_device_type_t type = ahci_port_get_type(port);

            g_ports[i].port_number = (uint8_t)i;
            g_ports[i].hba_port    = port;
            g_ports[i].type        = type;
            g_ports[i].present     = false;

            if (type == AHCI_DEV_SATA || type == AHCI_DEV_SATAPI) {
                void* phys_mem = kmalloc_aligned(4096, 1024);
                ahci_port_rebase(&g_ports[i], i, (uintptr_t)phys_mem, (uintptr_t)phys_mem);

                if (ahci_port_identify(&g_ports[i])) {
                    /* Disk başarıyla id edildikten sonra Block Layer'a kaydet */
                    block_device_t bdev;
                    for (int k = 0; k < BLOCK_DEV_NAME_MAX; k++) bdev.name[k] = 0;
                    
                    /* Model ismini veya varsayılan SATA adını ata */
                    if (g_ports[i].model[0] != '\0') {
                        for (int k = 0; k < 32 && g_ports[i].model[k]; k++) {
                            bdev.name[k] = g_ports[i].model[k];
                        }
                    } else {
                        bdev.name[0] = 'S'; bdev.name[1] = 'A'; bdev.name[2] = 'T'; 
                        bdev.name[3] = 'A'; bdev.name[4] = ' '; bdev.name[5] = 'S'; 
                        bdev.name[6] = 'S'; bdev.name[7] = 'D';
                    }

                    bdev.type          = BLOCK_DEV_TYPE_AHCI_SATA;
                    bdev.sector_size   = g_ports[i].sector_size;
                    bdev.sector_count  = g_ports[i].capacity_sectors;
                    bdev.flags         = 0;
                    bdev.ops           = &g_ahci_block_ops;
                    bdev.private_data  = &g_ports[i];

                    /* Block katmanına aygıtı kaydet */
                    block_register(&bdev);
                }
            }
        }
    }
}

bool ahci_probe(ahci_pci_device_t* pci_dev) {
    if (!pci_dev || pci_dev->abar_phys == 0) return false;

    g_abar = (hba_mem_t*)pci_dev->abar_phys;

    kprintf("[AHCI]\n");
    kprintf("Controller Found\n");
    kprintf("Vendor:%x\n", pci_dev->vendor_id);
    kprintf("ABAR:%x\n", (uint32_t)pci_dev->abar_phys);

    /* AHCI Enable & Global Controller Reset Check */
    g_abar->ghc |= AHCI_GHC_AE;

    ahci_scan_ports();
    return true;
}

void ahci_init(void) {
    for (int i = 0; i < AHCI_MAX_PORTS; i++) {
        g_ports[i].present = false;
        g_ports[i].type = AHCI_DEV_NULL;
    }
}

ahci_port_device_t* ahci_find_port(uint8_t port_no) {
    if (port_no >= AHCI_MAX_PORTS) return NULL;
    if (!g_ports[port_no].present) return NULL;
    return &g_ports[port_no];
}

bool ahci_identify(uint8_t port_no) {
    ahci_port_device_t* dev = ahci_find_port(port_no);
    if (!dev) return false;
    return ahci_port_identify(dev);
}

bool ahci_read(uint8_t port_no, uint64_t start_lba, uint32_t count, uint8_t* buffer) {
    ahci_port_device_t* dev = ahci_find_port(port_no);
    if (!dev) return false;
    return ahci_port_read(dev, start_lba, count, buffer);
}

bool ahci_write(uint8_t port_no, uint64_t start_lba, uint32_t count, const uint8_t* buffer) {
    ahci_port_device_t* dev = ahci_find_port(port_no);
    if (!dev) return false;
    return ahci_port_write(dev, start_lba, count, buffer);
}

bool ahci_reset_port(uint8_t port_no) {
    if (port_no >= AHCI_MAX_PORTS) return false;
    if (!g_ports[port_no].hba_port) return false;
    return ahci_port_reset(g_ports[port_no].hba_port);
}