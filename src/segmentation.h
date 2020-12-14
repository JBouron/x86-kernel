#pragma once
#include <types.h>
#include <smp.h>
#include <interrupt.h>

// Initialize the segmentation / GDT. This function will "jump" to higher half
// logical addresses (where the kernel has been linked to). This function does
// NOT RETURN, it jumps to the target which must be an higher half address.
// @param target: The destination of the jump in the new higher half code
// segment.
void init_segmentation(void * const target);

// Fixup the GDT after paging has been enabled. This function should be called
// when paging is enabled and both the ID mapping and the higher half mapping
// are available.
void fixup_gdt_after_paging_enable(void);

// Init the segmentation mechanism for an Application Processor. This function
// will load the GDTR of the AP with the address of the current kernel-wide GDT
// and set the code and data segments accordingly.
void ap_init_segmentation(void);

// Setup the TSS on the current cpu. This function must be called after percpu
// variables are initialized and the cpu is using the final GDT.
void setup_tss(void);

// Get the segment selector for the kernel data segment.
// @return: The segment selector for the kernel data segment.
union segment_selector_t kernel_data_selector(void);

// Get the segment selector for the kernel code segment.
// @return: The segment selector for the kernel code segment.
union segment_selector_t kernel_code_selector(void);

// Initialize the temporary GDT used by the AP on start up.
// @param data_frame: The data frame in which the GDT resides.
void initialize_trampoline_gdt(struct ap_boot_data_frame * const data_frame);

// Allocate the final GDT and load it into the GDTR.
void init_final_gdt();

// Get the segment selector that should be used for the user space code segment.
// @return: The correct selector.
union segment_selector_t user_code_seg_sel(void);

// Get the segment selector that should be used for the user space data segment.
// @return: The correct selector.
union segment_selector_t user_data_seg_sel(void);

// Set all the segment registers for kernel execution. This is used when
// entering the kernel through an interrupt.
void set_segment_registers_for_kernel(void);

// Change the stack pointer for the cpu's TSS.
// @param new_esp0: The new esp0 to use.
void change_tss_esp0(void const * const new_esp0);

// Get the current ESP0 in the TSS of a cpu.
// @param cpu: The cpu to get the ESP0 for.
// @return: The value of ESP0 in the cpu's TSS.
// Note: This function is only used for testing.
void *get_tss_esp0(uint8_t const cpu);

// Interrupt callback for a double fault.
// @param frame: The interrupt frame. Note that in the case of a double this is
// not used, the handler will look at the previous TSS instead to get the state
// at the time of the interrupt.
void double_fault_panic(struct interrupt_frame const * const frame);

// Run tests related to segmentation.
void segmentation_test(void);
