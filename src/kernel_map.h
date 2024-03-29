#pragma once
#include <cpu.h>

// This file contains the memory locations of all the section of the kernel's
// executable.

// The following symbols are defined in the linker.ld script. Those are not
// meant to be used directly. Use the macros defined down below to get the
// addresses.
extern uint8_t KERNEL_PHY_OFFSET;
extern uint8_t KERNEL_START;
extern uint8_t SECTION_TEXT_START;
extern uint8_t SECTION_TEXT_END;
extern uint8_t SECTION_RODATA_START;
extern uint8_t SECTION_RODATA_END;
extern uint8_t SECTION_DATA_START;
extern uint8_t SECTION_DATA_END;
extern uint8_t SECTION_BSS_START;
extern uint8_t SECTION_BSS_END;
extern uint8_t SECTION_PERCPU_START;
extern uint8_t SECTION_PERCPU_END;
extern uint8_t KERNEL_END;

// The start and end _virtual_ addresses of the kernel's section as well as the
// kernel itself.
#define KERNEL_PHY_OFFSET            (&KERNEL_PHY_OFFSET)
#define KERNEL_START_ADDR            (&KERNEL_START)
#define SECTION_TEXT_START_ADDR      (&SECTION_TEXT_START)
#define SECTION_TEXT_END_ADDR        (&SECTION_TEXT_END - 1)
#define SECTION_RODATA_START_ADDR    (&SECTION_RODATA_START)
#define SECTION_RODATA_END_ADDR      (&SECTION_RODATA_END - 1)
#define SECTION_DATA_START_ADDR      (&SECTION_DATA_START)
#define SECTION_DATA_END_ADDR        (&SECTION_DATA_END - 1)
#define SECTION_BSS_START_ADDR       (&SECTION_BSS_START)
#define SECTION_BSS_END_ADDR         (&SECTION_BSS_END - 1)
#define SECTION_PERCPU_START_ADDR    (&SECTION_PERCPU_START)
#define SECTION_PERCPU_END_ADDR      (&SECTION_PERCPU_END - 1)
#define KERNEL_END_ADDR              (&KERNEL_END - 1)

// The size of the kernel's sections and the kernel itself.
#define KERNEL_SIZE \
    (KERNEL_END_ADDR - KERNEL_START_ADDR + 1)
#define SECTION_TEXT_SIZE \
    (SECTION_TEXT_END_ADDR - SECTION_TEXT_START_ADDR + 1)
#define SECTION_RODATA_SIZE \
    (SECTION_RODATA_END_ADDR - SECTION_RODATA_START_ADDR + 1)
#define SECTION_DATA_SIZE \
    (SECTION_DATA_END_ADDR - SECTION_DATA_START_ADDR + 1)
#define SECTION_BSS_SIZE \
    (SECTION_BSS_END_ADDR - SECTION_BSS_START_ADDR + 1)
#define SECTION_PERCPU_SIZE \
    (SECTION_PERCPU_END_ADDR - SECTION_PERCPU_START_ADDR + 1)

// Translate a virtual pointer to a physical one. Note that this function should
// be used when setting paging. Once paging is set this function will not return
// valid results.
// @param ptr: The pointer to transform.
// @return: The corresponding virtual pointer.
void * to_phys(void const * const ptr);

// Translate a physical pointer to a virtual one. Note that this function should
// be used when setting paging. Once paging is set this function will not return
// valid results.
// @param ptr: The pointer to transform.
// @return: The corresponding virtual pointer.
void * to_virt(void const * const ptr);

// Get the pointer in the current address space for a variable. This functions
// is useful for code that can operate with paging both enabled and disabled.
// @param var: The name of the variable to get a pointer to.
// @evaluate: If paging is enabled, the macro/expression evaluates to the
// virtual address of the variable `var`, otherwise it evaluates to the physical
// address.
#define PTR(var) \
    ((typeof(&var))(cpu_paging_enabled() ? &var : to_phys(&var)))

// Check if a pointer in a given range.
// @param ptr: The pointer to test.
// @param start: The lower bound of the range.
// @param end: The higher bound of the range.
// @return: True if ptr is in the range [start, end] (incl.), false otherwise.
static inline bool ptr_in_range(void const * const ptr,
                                void const * const start,
                                void const * const end) {
    return start <= ptr && ptr <= end;
}

// Check if a virtual pointer is within the .text section of the kernel.
// @param p: The pointer to test.
// @return: True if p is within the .text section of the kernel, false
// otherwise.
static inline bool in_text_section(void const * const p) {
    return ptr_in_range(p, SECTION_TEXT_START_ADDR, SECTION_TEXT_END_ADDR);
}

// Check if a virtual pointer is within the .rodata section of the kernel.
// @param p: The pointer to test.
// @return: True if p is within the .rodata section of the kernel, false
// otherwise.
static inline bool in_rodata_section(void const * const p) {
    return ptr_in_range(p, SECTION_RODATA_START_ADDR, SECTION_RODATA_END_ADDR);
}

// Check if a virtual pointer points to the mapped low 1MiB of physical memory,
// eg. from KERNEL_PHY_OFFSET, KERNEL_PHY_OFFSET + 1MiB.
// @param p: The pointer to test.
// @return: True if p is within the mapped low 1MiB of physical memory, false
// otherwise.
static inline bool in_low_mem(void const * const p) {
    ptrdiff_t const diff = p - (void*)KERNEL_PHY_OFFSET;
    return diff < (1 << 20);
}

// Check if a pointer points to an higher half kernel, that is anything above or
// equal to address KERNEL_PHY_OFFSET.
// @param p: The pointer to test.
// @return: True if p >= KERNEL_PHY_OFFSET, false otherwise.
static inline bool is_higher_half(void const * const p) {
    return ptr_in_range(p, KERNEL_PHY_OFFSET, (void*)(~((uint32_t)0)));
}

// Check if a pointer is 4KiB aligned.
// @param p: The pointer to test.
// @return: true if p is 4KiB aligned, false otherwise.
static inline bool is_4kib_aligned(void const * const p) {
    uint32_t const offset = (uint32_t)p;
    return !(offset & 0xFFF);
}

// Get the (virtual) address of the virtual page containing an address.
// @param ptr: The address to get its page address from.
// @return: A 4KiB aligned address corresponding to the page containing `ptr`.
void * get_page_addr(void const * const ptr);

// Test if an address is a kernel address.
// @param addr: The address to test.
// @return: true if `addr` is a kernel address (that is in the range
// KERNEL_PHY_OFFSET to 0xFFFFFFFF), false otherwise.
static inline bool is_kernel_addr(void const * const addr) {
    return (void*)KERNEL_PHY_OFFSET <= addr;
}

// Test if an address is a userspace address.
// @param addr: The address to test.
// @return: true if `addr` is a userspace address (that is in the range
// 0x00000000 to KERNEL_PHY_OFFSET-1), false otherwise.
static inline bool is_user_addr(void const * const addr) {
    return !is_kernel_addr(addr);
}

// Test if the current execution is in higher half addresses.
// @return: true if EIP is > KERNEL_PHY_OFFSET, false otherwise.
static inline bool in_higher_half(void) {
    return cpu_read_eip() > (void*)KERNEL_PHY_OFFSET;
}
