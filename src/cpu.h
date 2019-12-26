#pragma once
#include <types.h>

// CPU related operations.

// Read a Model-Specific-Register into dest.
// @param msr_num: The MSR number, as found in Intel's documentation.
// @return: The value read from the MSR.
uint64_t read_msr(uint32_t const msr_num);

// Write in an MSR.
// @param msr_num: The MSR number, as found in Intel's documentation.
// @param val: The value to write into the MSR.
void write_msr(uint32_t const msr_num, uint64_t const val);

// Read the Timestamp counter.
// @return: The value of the TSC
uint64_t read_tsc(void);

// Lock up the cpu by disabling the interrupts and halting.
void lock_up(void);

// Test whether or not the current CPU supports the CPUID instruction. This is
// done using the ID flags in EFLAGS (see manual page 765).
// @return: True iff the CPU supports CPUID.
bool has_cpuid(void);

// Execute the CPUID instruction with a specific value of EAX, ECX is left to 0.
// If ECX is needed, use the cpuid_ecx variant.
// @param i_eax: The value of EAX to use when calling CPUID.
// @param o_eax [out]: The address to write the value of EAX to after, the call
// to CPUID.
// @param o_ebx [out]: The address to write the value of EBX to after, the call
// to CPUID.
// @param o_ecx [out]: The address to write the value of ECX to after, the call
// to CPUID.
// @param o_edx [out]: The address to write the value of EDX to after, the call
// to CPUID.
// Note: Any of the output pointers can be NULL, in which case the value is
// ignored.
void cpuid(uint32_t i_eax,
           uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx, uint32_t *o_edx);

// Execute the CPUID instruction with specific values for EAX and ECX.
// @param i_eax: The value of EAX to use when calling CPUID.
// @param i_ecx: The value of ECX to use when calling CPUID.
// @param o_eax [out]: The address to write the value of EAX to after, the call
// to CPUID.
// @param o_ebx [out]: The address to write the value of EBX to after, the call
// to CPUID.
// @param o_ecx [out]: The address to write the value of ECX to after, the call
// to CPUID.
// @param o_edx [out]: The address to write the value of EDX to after, the call
// to CPUID.
// Note: Any of the output pointers can be NULL, in which case the value is
// ignored.
void cpuid_ecx(uint32_t i_eax, uint32_t i_ecx,
               uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx,
               uint32_t *o_edx);

// Write a byte to an I/O port.
// @param port: The port to write the byte to.
// @param byte: The value of the byte to write to the port.
void cpu_outb(uint16_t const port, uint8_t const byte);

// Write a word to an I/O port.
// @param port: The port to write the word to.
// @param byte: The value of the word to write to the port.
void cpu_outw(uint16_t const port, uint16_t const word);

// Read a byte from an I/O port.
// @param port: The port to read a byte from.
uint8_t cpu_inb(uint16_t const port);

// Desciptor for a GDT containing the size and base address of the GDT.
struct gdt_desc_t {
    // Limit is such that base+limit points to the latest valid byte of the GDT.
    // Therefore limit should have the form 8N - 1, for N being the number of
    // elements in the GDT.
    uint16_t limit;
    // The base address of the GDT.
    void* base;
} __attribute__((packed));

// Load a GDT for the cpu to use.
// @param table_desc: Descriptor of the GDT containing size and base address.
void cpu_lgdt(struct gdt_desc_t const * const table_desc);

// Store the GDTR into a memory address.
// @param dest: The destination to write the GDTR into.
void cpu_sgdt(struct gdt_desc_t * const dest);

// Segment selector. This value is written in segment register to use a
// particular segment.
union segment_selector_t {
	uint16_t value;
	struct {
        // Requested privilege level to access the segment.
        uint8_t requested_priv_level : 2;
        // If set this bit indicates that the cpu look up the index in the LDT
        // instead.
        uint8_t is_local : 1;
        // Index of the segment to use in the GDT.
        uint16_t index : 13;
    } __attribute__((packed));
} __attribute__((packed));

// Set a new value for the code segment.
// @param sel: The segment selector to store in %CS.
void cpu_set_cs(union segment_selector_t const * const sel);

// Set a new value for DS.
// @param sel: The segment selector to store in DS.
void cpu_set_ds(union segment_selector_t const * const sel);

// Set a new value for ES.
// @param sel: The segment selector to store in ES.
void cpu_set_es(union segment_selector_t const * const sel);

// Set a new value for FS.
// @param sel: The segment selector to store in FS.
void cpu_set_fs(union segment_selector_t const * const sel);

// Set a new value for GS.
// @param sel: The segment selector to store in GS.
void cpu_set_gs(union segment_selector_t const * const sel);

// Set a new value for SS.
// @param sel: The segment selector to store in SS.
void cpu_set_ss(union segment_selector_t const * const sel);

// Read the CS register.
// @return: Current value of CS.
union segment_selector_t cpu_read_cs(void);

// Read the DS register.
// @return: Current value of DS.
union segment_selector_t cpu_read_ds(void);

// Read the ES register.
// @return: Current value of ES.
union segment_selector_t cpu_read_es(void);

// Read the FS register.
// @return: Current value of FS.
union segment_selector_t cpu_read_fs(void);

// Read the GS register.
// @return: Current value of GS.
union segment_selector_t cpu_read_gs(void);

// Read the SS register.
// @return: Current value of SS.
union segment_selector_t cpu_read_ss(void);

// Execute tests on CPU related functions.
void cpu_test(void);
