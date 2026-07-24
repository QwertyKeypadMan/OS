#include "kernel/driver/driver.h"
#include "kernel/driver/driver_registry.h"

extern void kprintf(const char* fmt, ...);

/* =========================================================================
 * 1. DUMMY STORAGE DRIVER (Class Code 0x01: Mass Storage)
 * ========================================================================= */
static int dummy_storage_probe(pci_device_t* dev) {
    (void)dev;
    return 0; /* Her zaman kabul et */
}

static int dummy_storage_init(pci_device_t* dev) {
    (void)dev;
    kprintf("[Driver] Dummy Storage Driver attached\n");
    return 0;
}

static driver_t g_dummy_storage_driver = {
    .name                 = "Dummy Storage Driver",
    .version              = "1.0.0",
    .author               = "KayaOS Team",
    .supported_vendor_id  = PCI_ANY_ID,
    .supported_device_id  = PCI_ANY_ID,
    .supported_class_code = 0x01, /* Mass Storage Class */
    .supported_subclass   = PCI_ANY_CLASS,
    .priority             = 10,
    .probe                = dummy_storage_probe,
    .init                 = dummy_storage_init,
    .shutdown             = NULL,
    .remove               = NULL
};

/* =========================================================================
 * 2. GENERIC PCI DRIVER (Özel olarak Intel QEMU / i440FX Host Bridge için)
 * ========================================================================= */
static int generic_pci_init(pci_device_t* dev) {
    (void)dev;
    kprintf("[Driver] Generic PCI Driver attached\n");
    return 0;
}

static driver_t g_generic_pci_driver = {
    .name                 = "Generic PCI Driver",
    .version              = "1.0.0",
    .author               = "KayaOS Team",
    .supported_vendor_id  = 0x8086, /* Intel */
    .supported_device_id  = 0x1237, /* i440FX PMC */
    .supported_class_code = PCI_ANY_CLASS,
    .supported_subclass   = PCI_ANY_CLASS,
    .priority             = 100, /* Tam ID eşleşmesi olduğu için yüksek öncelik */
    .probe                = NULL,
    .init                 = generic_pci_init,
    .shutdown             = NULL,
    .remove               = NULL
};

/* =========================================================================
 * 3. UNKNOWN DEVICE FALLBACK DRIVER (Her cihaza uyan en düşük öncelikli sürücü)
 * ========================================================================= */
static int unknown_device_init(pci_device_t* dev) {
    (void)dev;
    kprintf("[Driver] Unknown Device Driver attached (Generic Fallback)\n");
    return 0;
}

static driver_t g_unknown_device_driver = {
    .name                 = "Unknown Device Driver",
    .version              = "0.1.0",
    .author               = "KayaOS Team",
    .supported_vendor_id  = PCI_ANY_ID,
    .supported_device_id  = PCI_ANY_ID,
    .supported_class_code = PCI_ANY_CLASS,
    .supported_subclass   = PCI_ANY_CLASS,
    .priority             = 1, /* En düşük öncelik */
    .probe                = NULL,
    .init                 = unknown_device_init,
    .shutdown             = NULL,
    .remove               = NULL
};

/* Driver Manager kaydı örneği */


/* Tüm Dummy sürücüleri sisteme kaydetme fonksiyonu */
void register_dummy_drivers(void) {
    driver_register(&g_generic_pci_driver);
    driver_register(&g_dummy_storage_driver);
    driver_register(&g_unknown_device_driver);
}