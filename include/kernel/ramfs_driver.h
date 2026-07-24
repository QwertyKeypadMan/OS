#ifndef KERNEL_RAMFS_DRIVER_H
#define KERNEL_RAMFS_DRIVER_H

#include "kernel/vfs_driver.h"

vfs_driver_t *ramfs_get_driver(void);

#endif /* KERNEL_RAMFS_DRIVER_H */