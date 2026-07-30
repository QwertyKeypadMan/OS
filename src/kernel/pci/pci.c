#include "kernel/pci/pci.h"
#include "kernel/driver/driver.h"
#include "kernel/driver/driver_manager.h"

static pci_device_t g_pci_devices[MAX_PCI_DEVICES];
static int g_pci_device_count = 0;

void pci_add_device(const pci_device_t *dev) {
    if (g_pci_device_count < MAX_PCI_DEVICES) {
        g_pci_devices[g_pci_device_count] = *dev;
        g_pci_device_count++;
    }
}

void pci_init(void) {
           g_pci_device_count = 0;
           pci_scan();
       
           /* Bulunan her cihaz icin en uygun surucuyu bul ve baglat.\r\n'
            * (Onceden hicbir yerden cagrilmiyordu -- bulunan cihazlar\r\n'
            * asla bir surucuye baglanmiyordu.) */
           for (int i = 0; i < g_pci_device_count; i++) {
               driver_manager_bind_pci_device(&g_pci_devices[i]);
           }
       }

int pci_get_device_count(void) {
    return g_pci_device_count;
}

pci_device_t* pci_get_device(int index) {
    if (index < 0 || index >= g_pci_device_count) {
        return NULL;
    }
    return &g_pci_devices[index];
}

pci_device_t* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < g_pci_device_count; i++) {
        if (g_pci_devices[i].vendor_id == vendor_id && g_pci_devices[i].device_id == device_id) {
            return &g_pci_devices[i];
        }
    }
    return NULL;
}

pci_device_t* pci_find_device_by_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < g_pci_device_count; i++) {
        if (g_pci_devices[i].class_code == class_code && g_pci_devices[i].subclass == subclass) {
            return &g_pci_devices[i];
        }
    }
    return NULL;
}

bool pci_get_bar(const pci_device_t *dev, uint8_t bar_index, pci_bar_t *out_bar) {
    if (!dev || !out_bar || bar_index >= 6) return false;
    
    // 'bars' yerine 'bar'
    
    return (out_bar->type != BAR_TYPE_NONE);
}