#include <percpu.h>
#include <segmentation.h>
#include <acpi.h>
#include <kmalloc.h>
#include <memory.h>

// Allocate one per-cpu variable area per cpu on the system and initialize basic
// variables (defined above).
static void allocate_per_cpu_areas(void) {
    uint8_t const ncpus = acpi_get_number_cpus();

    // Allocate the PER_CPU_OFFSETS.
    PER_CPU_OFFSETS = kmalloc(ncpus * sizeof(*PER_CPU_OFFSETS));

    // The size of the segment to contain the per-cpu variables is given by the
    // suze of the .percpu section which contain a set of those variables.
    size_t const size = SECTION_PERCPU_SIZE;

    // Allocate, for each cpu, a memory area to contain its private variables.
    // Register the offsets in PER_CPU_OFFSETS.
    for (uint8_t i = 0; i < ncpus; ++i) {
        void * const cpu_var_area = kmalloc(size);

        PER_CPU_OFFSETS[i] = cpu_var_area;

        // Initialize basic vars.
        cpu_var(this_cpu_off, i) = cpu_var_area;
        cpu_var(cpu_id, i) = i;

        LOG("Per cpu data for cpu %u @ %p\n", i, cpu_var_area);
    }
}

// Has the percpu system been initialized ?
static bool PERCPU_INITIALIZED = false;

void init_percpu(void) {
    allocate_per_cpu_areas();
    PERCPU_INITIALIZED = true;
}

bool percpu_initialized(void) {
    // If %GS is zero then this cpu is not ready to use percpu data.
    return cpu_read_gs().value != 0x0 && PERCPU_INITIALIZED;
}

#include <percpu.test>
