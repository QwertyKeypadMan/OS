#ifndef KAYAOS_DRIVER_MANAGER_H
#define KAYAOS_DRIVER_MANAGER_H

#include "driver.h"

void driver_manager_init(void);

/* PCI Taramasında bulunan her bir cihaz için çağrılır */
int driver_manager_bind_pci_device(pci_device_t* dev);

/* Tesis edilen tüm sürücüleri kapatır */
void driver_manager_shutdown_all(void);

#endif /* KAYAOS_DRIVER_MANAGER_H */