#include <percpu.h>
#include <segmentation.h>
#include <acpi.h>
#include <kmalloc.h>
#include <memory.h>

void allocate_aps_percpu_areas(void) {
    uint8_t const ncpus = acpi_get_number_cpus();

    // Allocate the PER_CPU_OFFSETS.
    PER_CPU_OFFSETS = kmalloc(ncpus * sizeof(*PER_CPU_OFFSETS));
    if (!PER_CPU_OFFSETS) {
        // There is no point in continuing if we can't allocate percpu data.
        PANIC("Cannot allocate percpu metadata\n");
    }

    // The size of the segment to contain the per-cpu variables is given by the
    // suze of the .percpu section which contain a set of those variables.
    size_t const size = SECTION_PERCPU_SIZE;

    // Allocate, for each cpu, a memory area to contain its private variables.
    // Register the offsets in PER_CPU_OFFSETS.
    for (uint8_t i = 0; i < ncpus; ++i) {
        if (i == cpu_id()) {
            // This is the BSP percpu segment, this segment as already been
            // allocated (statically) and variables have been initialized.
            PER_CPU_OFFSETS[i] = this_cpu_var(this_cpu_off);
        } else {
            void * const cpu_var_area = kmalloc(size);
            if (!cpu_var_area) {
                PANIC("Cannot allocate percpu area for cpu %u\n", i);
            }

            PER_CPU_OFFSETS[i] = cpu_var_area;

            // Initialize basic vars.
            cpu_var(this_cpu_off, i) = cpu_var_area;
            cpu_var(cpu_id, i) = i;

            LOG("Per cpu data for cpu %u @ %p\n", i, cpu_var_area);
        }
    }
}

void init_bsp_boot_percpu(void) {
    ASSERT(!cpu_paging_enabled());
    ASSERT(in_higher_half());

    LOG("Initializing BSP percpu segment.\n");

    // The base (and therefore this_cpu_off) here is the virtual address of the
    // .percpu section. This is because the current GDT already maps the higher
    // half addresses.
    void * const base = SECTION_PERCPU_START_ADDR;
    LOG("BSP's percpu segment located at logical address %p.\n", base);

    // In order to be able to use the this_cpu_var() macro, the `this_cpu_off`
    // variable must be set in the percpu segment of the current cpu.
    void ** const this_cpu_off_ptr = (void*)base + _VAR_OFFSET(this_cpu_off);
    *this_cpu_off_ptr = (void*)base;

    // We can now use this_cpu_var().
    this_cpu_var(cpu_id) = cpu_apic_id();

    extern uint8_t stack_top;
    this_cpu_var(kernel_stack) = &stack_top;
}

#include <percpu.test>
