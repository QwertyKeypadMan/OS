#ifndef KAYAOS_DRIVER_REGISTRY_H
#define KAYAOS_DRIVER_REGISTRY_H

#include "driver.h"

void driver_registry_init(void);
int  driver_register(driver_t* drv);
int  driver_unregister(driver_t* drv);

/* PCI cihazı için en uygun sürücüyü bulur */
driver_t* driver_find_best_match(pci_device_t* dev);

/* Bütün kayıtlı sürücüleri loglar */
void driver_registry_dump(void);

#endif /* KAYAOS_DRIVER_REGISTRY_H */