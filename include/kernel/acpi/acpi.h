#ifndef ACPI_H
#define ACPI_H

#include "acpi_tables.h"
#include "acpi_power.h"

bool acpi_init(void);
bool acpi_enable(void);

#endif