#pragma once
#include <types.h>
#include <smp.h>

// Initialize the segmentation feature / GDT.
// This kernel uses a flat model for segmentation in which there are 2 segments
// per privilege level covering the entire linear address space (0 to 4GiB):
// code and data.
void init_segmentation(void);

// Init the segmentation mechanism for an Application Processor. This function
// will load the GDTR of the AP with the address of the current kernel-wide GDT
// and set the code and data segments accordingly.
void ap_init_segmentation(void);

// Get the segment selector for the kernel data segment.
// @return: The segment selector for the kernel data segment.
union segment_selector_t kernel_data_selector(void);

// Get the segment selector for the kernel code segment.
// @return: The segment selector for the kernel code segment.
union segment_selector_t kernel_code_selector(void);

// Initialize the temporary GDT used by the AP on start up.
// @param data_frame: The data frame in which the GDT resides.
void initialize_trampoline_gdt(struct ap_boot_data_frame_t * const data_frame);

// Allocate the final GDT and load it into the GDTR.
// @param per_cpu_areas: Array containing one void* per cpu on the system
// pointing to their private per-cpu areas. Those areas will be transformed into
// segment in the final GDT.
void switch_to_final_gdt(void **per_cpu_areas);

// Run tests related to segmentation.
void segmentation_test(void);
