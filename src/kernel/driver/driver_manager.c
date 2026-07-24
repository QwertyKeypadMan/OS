#include "kernel/driver/driver_manager.h"
#include "kernel/driver/driver_registry.h"

extern void kprintf(const char* fmt, ...);

/* Test için Dummy Sürücüleri bildiren fonksiyon */
extern void register_dummy_drivers(void);

void driver_manager_init(void) {
    kprintf("[DriverManager] Initializing Driver Subsystem...\n");
    driver_registry_init();
    
    /* Dahili / Dummy sürücüleri registry'ye yükle */
    register_dummy_drivers();
    
    kprintf("[DriverManager] Subsystem ready.\n");
}

int driver_manager_bind_pci_device(pci_device_t* dev) {
    if (!dev) return -1;

    kprintf("\n[PCI]\n");
    kprintf("Device Found -> Vendor: %04X | Device: %04X | Class: %02X SubClass: %02X\n",
            dev->vendor_id, dev->device_id, dev->class_code, dev->subclass);

    kprintf("[DriverManager] Searching driver...\n");

    driver_t* matched_driver = driver_find_best_match(dev);

    if (matched_driver) {
        kprintf("[DriverManager] Matched: %s\n", matched_driver->name);
        
        /* Cihaza sürücü bağla */
        dev->driver = matched_driver;

        /* Sürücüyü başlat */
        if (matched_driver->init) {
            int status = matched_driver->init(dev);
            if (status == 0) {
                kprintf("[DriverManager] Driver Init OK\n");
                return 0;
            } else {
                kprintf("[DriverManager] Driver Init Failed with code: %d\n", status);
                dev->driver = NULL;
                return status;
            }
        } else {
            kprintf("[DriverManager] Driver Init OK (No init payload)\n");
            return 0;
        }
    } else {
        kprintf("[DriverManager] No specific driver found for device.\n");
        return -1;
    }
}

void driver_manager_shutdown_all(void) {
    kprintf("[DriverManager] Shutting down all active drivers...\n");
    /* Gerçek donanım kapatma işlemleri ileride buraya eklenecektir */
}