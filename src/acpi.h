#pragma once

// Functions related to ACPI parsing.

// Read the ACPI table.
void acpi_init(void);

// Get the address of the I/O APIC.
// @return: The physical address of the IO APIC.
void * acpi_get_ioapic_addr(void);

// Get the IO APIC source vector for a legacy ISA interrupt vector.
// @param isa_vector: The ISA vector for which this function should return the
// corresponding IO APIC vector.
// @return: The IO APIC source vector that correspond to `isa_vector`.
uint8_t acpi_get_isa_interrupt_vector_mapping(uint8_t const isa_vector);

// Get the number of cpus on the system as described by the ACPI tables.
// @return: The number of cpus found in the MADT.
uint16_t acpi_get_number_cpus(void);

// Get a pointer on the MCFG table for the PCIe segment. The layout of this
// table is not given on purpose to avoid layer violations.
void *get_mcfg_table(void);
