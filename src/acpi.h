#pragma once

// Read the ACPI table.
void acpi_init(void);

// Get the address of the I/O APIC.
// @return: The physical address of the IO APIC.
void * acpi_get_ioapic_addr(void);
