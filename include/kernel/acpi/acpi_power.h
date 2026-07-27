#ifndef ACPI_POWER_H
#define ACPI_POWER_H

#include <stdint.h>
#include "kernel/io.h" /* Port I/O fonksiyonları buradan çekilsin */
#include "acpi.h"      /* fadt_t vb. tipler için */

void acpi_power_init(fadt_t* fadt_ptr);
void acpi_shutdown(void);
void acpi_reboot(void);

#endif /* ACPI_POWER_H */