#include "kernel/driver/driver_registry.h"

/* Basit bir kütüphane/kernel log yardımcısı varsayımı */
extern void kprintf(const char* fmt, ...);

static driver_t* g_driver_list_head = NULL;

void driver_registry_init(void) {
    g_driver_list_head = NULL;
}

int driver_register(driver_t* drv) {
    if (!drv || !drv->name) {
        return -1;
    }

    /* Aynı sürücünün tekrar eklenmesini önle */
    driver_t* curr = g_driver_list_head;
    while (curr) {
        if (curr == drv) {
            return 0; /* Zaten kayıtlı */
        }
        curr = curr->next;
    }

    /* Listenin başına ekle */
    drv->next = g_driver_list_head;
    g_driver_list_head = drv;

    kprintf("[DriverRegistry] Registered: %s (v%s)\n", drv->name, drv->version);
    return 0;
}

int driver_unregister(driver_t* drv) {
    if (!drv || !g_driver_list_head) return -1;

    if (g_driver_list_head == drv) {
        g_driver_list_head = drv->next;
        drv->next = NULL;
        return 0;
    }

    driver_t* curr = g_driver_list_head;
    while (curr->next) {
        if (curr->next == drv) {
            curr->next = drv->next;
            drv->next = NULL;
            return 0;
        }
        curr = curr->next;
    }

    return -1;
}

/* Sürücü ve Cihaz Eşleşme Puanlama Mantığı */
static uint32_t calculate_match_score(driver_t* drv, pci_device_t* dev) {
    uint32_t score = 0;

    /* 1. Tam Vendor ID & Device ID Eşleşmesi (En yüksek öncelik) */
    if (drv->supported_vendor_id != PCI_ANY_ID && drv->supported_vendor_id == dev->vendor_id) {
        if (drv->supported_device_id != PCI_ANY_ID && drv->supported_device_id == dev->device_id) {
            score += 1000;
        } else if (drv->supported_device_id == PCI_ANY_ID) {
            score += 500;
        } else {
            return 0; /* Vendor tuttu ama Device ID uyuşmadı */
        }
    } else if (drv->supported_vendor_id != PCI_ANY_ID) {
        return 0; /* Vendor tutmadı */
    }

    /* 2. Class & Subclass Eşleşmesi */
    if (drv->supported_class_code != PCI_ANY_CLASS) {
        if (drv->supported_class_code == dev->class_code) {
            score += 100;
            if (drv->supported_subclass != PCI_ANY_CLASS) {
                if (drv->supported_subclass == dev->subclass) {
                    score += 50;
                } else {
                    return 0; /* Class tuttu ama Subclass uymadı */
                }
            }
        } else {
            return 0; /* Class tutmadı */
        }
    }

    /* Sürücünün kendi taban önceliğini puana ekle */
    if (score > 0) {
        score += drv->priority;
    }

    return score;
}

driver_t* driver_find_best_match(pci_device_t* dev) {
    if (!dev) return NULL;

    driver_t* best_driver = NULL;
    uint32_t  highest_score = 0;

    driver_t* curr = g_driver_list_head;
    while (curr) {
        uint32_t score = calculate_match_score(curr, dev);
        if (score > highest_score) {
            /* Sürücünün özel probe() testi varsa çağır */
            if (curr->probe) {
                if (curr->probe(dev) == 0) {
                    highest_score = score;
                    best_driver = curr;
                }
            } else {
                highest_score = score;
                best_driver = curr;
            }
        }
        curr = curr->next;
    }

    return best_driver;
}

void driver_registry_dump(void) {
    kprintf("=== Registered Drivers ===\n");
    driver_t* curr = g_driver_list_head;
    while (curr) {
        kprintf(" - %s [%s] by %s (Priority: %u)\n", 
                curr->name, curr->version, curr->author, curr->priority);
        curr = curr->next;
    }
}