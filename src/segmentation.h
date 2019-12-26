#pragma once

// Initialize the segmentation feature / GDT.
// This kernel uses a flat model for segmentation in which there are 2 segments
// per privilege level covering the entire linear address space (0 to 4GiB):
// code and data.
void init_segmentation(void);

// Get the segment selector for the kernel data segment.
// @return: The segment selector for the kernel data segment.
union segment_selector_t kernel_data_selector(void);

// Get the segment selector for the kernel code segment.
// @return: The segment selector for the kernel code segment.
union segment_selector_t kernel_code_selector(void);

// Run tests related to segmentation.
void segmentation_test(void);
