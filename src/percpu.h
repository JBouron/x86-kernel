#pragma once
#include <types.h>
#include <kernel_map.h>

// Implementation of CPU-private variables.
// This implementation is inspired by the Linux implementation of per-cpu
// variables.
// The big idea is as follows:
//  - Every per-cpu variable declared (through the DECLARE_PER_CPU macro) is put
//  into the .percpu section of the executable.
//  - Each cpu has its own memory area containing its variables. This area is
//  the same size as the .percpu section.
//  - At initialization, the BSP allocates one per-cpu area per cpu on the
//  system.
//  - The GDT contains (besides the regular entries) entries for each allocated
//  per-cpu areas. Therefore each cpu has an entry in the GDT to access its
//  per-cpu private area. This makes accessing per-cpu variables fast as their
//  addresses can be computed with some segmentation magic and does not need to
//  execute heavy instructions such as cpuid.
//  - Each cpu uses the GS segment to point to its per-cpu private area.

// Implementation macros. These should only be used in the percpu
// implementation.

// Since the per-cpu variables are not supposed to be accessed directly but
// rather using the accessor macros. Thus obfuscate the name to avoid accidents.
#define _PERCPU_NAME(name) __percpu_##name

// Get the per-cpu variable's offset within the per-cpu area.
// @param name: The name of the per-cpu variable as declared with the
// DECLARE_PER_CPU macro.
#define _VAR_OFFSET(name) \
    (uint32_t)((void*)&_PERCPU_NAME(name) - (void*)SECTION_PERCPU_START_ADDR)

// Read a void* at an offset in the per-cpu segment of the current cpu.
// @param offset: The offset to read from.
// @return: The void* read at %gs:offset.
void *_read_void_ptr_at_offset(uint32_t const offset);

// Compute the pointer to this cpu's variable.
// @param var: The name of the variable to get a pointer to.
#define _THIS_CPU_VAR_PTR(var) \
    ((typeof(_PERCPU_NAME(var))*) \
     (_read_void_ptr_at_offset(_VAR_OFFSET(this_cpu_off))+_VAR_OFFSET(var)))

// Public interface:

// Declare a per-cpu variable.
// @param type: The type of the variable. Can be of any size, struct, ...
// @parma name: The name of the variable.
#define DECLARE_PER_CPU(type, name) \
    __attribute__((section(".percpu"))) type _PERCPU_NAME(name)

// Accessor macros:

// Get the value of this cpu's variable `var`. 
// @param var: The name of the variable, as declared using DECLARE_PER_CPU.
// Note: this macro can be used as a left and right value, to write and read
// respectively.
#define this_cpu_var(var) (*(_THIS_CPU_VAR_PTR(var)))

// Access a remote cpu's variable.
// @param var: The name of the variable, as declared using DECLARE_PER_CPU.
// @parma cpu: The cpu index.
// Note: this macro can be used as a left and right value, to write and read
// respectively.
#define cpu_var(var, cpu) \
    (*(typeof(_PERCPU_NAME(var))*)(PER_CPU_OFFSETS[cpu]+_VAR_OFFSET(var)))

// This array contains, for each cpu on the system, the address of the per-cpu
// variable area. This is allocated in the allocate_per_cpu_areas() function.
void **PER_CPU_OFFSETS;

// Initialize per-cpu variables.
void init_percpu(void);

// Execute per-cpu variables related tests.
void percpu_test(void);