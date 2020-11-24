// Define the Multiboot information datastructure passed by the boot-loader.
#pragma once
#include <types.h>

// Copyright (C) 1999,2003,2007,2008,2009,2010  Free Software Foundation, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL ANY
// DEVELOPER OR DISTRIBUTOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
// IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/* The symbol table for a.out. */
struct multiboot_aout_symbol_table {
    uint32_t tabsize;
    uint32_t strsize;
    uint32_t addr;
    uint32_t reserved;
}__attribute__((packed));

/* The section header table for ELF. */
struct multiboot_elf_section_header_table {
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
}__attribute__((packed));

struct multiboot_info {
	/* Multiboot info version number */
	uint32_t flags;

	/* Available memory from BIOS */
	uint32_t mem_lower;
	uint32_t mem_upper;

	/* "root" partition */
	uint32_t boot_device;

	/* Kernel command line */
	uint32_t cmdline;

	/* Boot-Module list */
	uint32_t mods_count;
	uint32_t mods_addr;

	union
	{
		struct multiboot_aout_symbol_table aout_sym;
		struct multiboot_elf_section_header_table elf_sec;
	} u;

	/* Memory Mapping buffer */
	uint32_t mmap_length;
	uint32_t mmap_addr;

	/* Drive Info buffer */
	uint32_t drives_length;
	uint32_t drives_addr;

	/* ROM configuration table */
	uint32_t config_table;

	/* Boot Loader Name */
	uint32_t boot_loader_name;

	/* APM table */
	uint32_t apm_table;

	/* Video */
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;

	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
#define MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB     1
#define MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT     2
	uint8_t framebuffer_type;
	union
	{
		struct
		{
			uint32_t framebuffer_palette_addr;
			uint16_t framebuffer_palette_num_colors;
		};
		struct
		{
			uint8_t framebuffer_red_field_position;
			uint8_t framebuffer_red_mask_size;
			uint8_t framebuffer_green_field_position;
			uint8_t framebuffer_green_mask_size;
			uint8_t framebuffer_blue_field_position;
			uint8_t framebuffer_blue_mask_size;
		};
	};
}__attribute__((packed));

struct multiboot_mmap_entry {
	uint32_t size;
	uint64_t base_addr;
    uint64_t length;
	uint32_t type;
}__attribute__((packed));

struct multiboot_mod_entry {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved; // Must be zero.
} __attribute__((packed));

// Initialize the multiboot helper functions. Note: This function _must_ be
// called before paging is enabled.
// @param ptr: A physical pointer on the multiboot info struct.
void init_multiboot(struct multiboot_info const * const ptr);

// Get a pointer to the multiboot structure.
// @return: A virtual pointer to the multiboot structure in RAM.
struct multiboot_info const *get_multiboot_info_struct(void);

// Get a pointer on the first entry of the mmap buffer in the multiboot info
// structure.
// @return: A physical pointer on the first entry of mmap.
struct multiboot_mmap_entry const *get_mmap_entry_ptr(void);

// Get the number of entries in the memory map buffer of the multiboot
// structure.
// @param: The number of entries.
uint32_t multiboot_mmap_entries_count(void);

// Check if an entry in the mmap buffer is available for use.
// @param entry: The entry to check.
// @return: true if the entry indicates a memory region that is available for
// use, false otherwise.
bool mmap_entry_is_available(struct multiboot_mmap_entry const * const entry);

// Check if a memory map entry describes a memory region that is within the
// first 4GiB of physical memory, that is the base address is below 4GiB.
// @param entry: The entry to test.
// @return: true if the memory is within 4GiB, false otherwise.
bool mmap_entry_within_4GiB(struct multiboot_mmap_entry const * const entry);

// Get the maximum address from a memory map entry.
// @param entry: The entry to get the maximum address from.
// @return: The physical address of the very last byte of the entry.
void * get_max_addr_for_entry(struct multiboot_mmap_entry const * const ent);

// The the maximum physical address available in RAM. That is the address of the
// very last available byte in RAM.
// @return: The address of the last byte in RAM.
void *get_max_addr(void);

// Find a contiguous memory area in physical RAM.
// @param len: The length in bytes of the memory area to find.
// @return: A physical pointer that is guaranteed to have `len` available,
// contiguous bytes.
void * find_contiguous_physical_frames(size_t const nframes);

// Get the start address of the initrd.
// @return: The physical address at which initrd starts.
void *multiboot_get_initrd_start(void);

// Get the size of the initrd.
// @return: The size of the initrd.
size_t multiboot_get_initrd_size(void);

// Execute tests related to the mutliboot struct manipulation functions.
void multiboot_test(void);
