#pragma once
#include <utils/types.h>
#include <utils/debug.h>
#include <memory/paging/alloc/alloc.h>

// Entry of a page directory.
struct pagedir_entry_t {
    // If this bit is 0 then the entry is considered not present and the
    // processor will throw an exception.
    uint8_t present : 1;
    // Enable writing on the memory covered by this entry.
    uint8_t writable : 1;
    // Make the region accessible with user privileges.
    uint8_t user_accessible : 1;
    // Enable write-through for this memory area.
    uint8_t write_through : 1;
    // Enable caching for this memory area.
    uint8_t cacheable : 1;
    // Set by the processor when this entry is accessed.
    uint8_t accessed : 1;
    // This field is ignored by the processor.
    uint8_t ignored : 1;
    // If this field is one then this entry correspond to a 4MiB page.
    uint8_t huge_page : 1;
    // This field is ignored as well.
    uint8_t ignored2 : 4;
    // Address of the page-table used. Must be 4KiB aligned.
    uint32_t pagetable_addr : 20;
} __attribute__((packed));

// Entry of a pagetable.
struct pagetable_entry_t {
    // If this bit is 0 then the entry is considered not present and the
    // processor will throw an exception.
    uint8_t present : 1;
    // Enable writing on the memory covered by this entry.
    uint8_t writable : 1;
    // Make the region accessible with user privileges.
    uint8_t user_accessible : 1;
    // Enable write-through for this memory area.
    uint8_t write_through : 1;
    // Enable caching for this memory area.
    uint8_t cacheable : 1;
    // Set by the processor when this entry is accessed.
    uint8_t accessed : 1;
    // Set by the processor on a write to the page.
    uint8_t dirty : 1;
    // PAT bit, ignored in this kernel, must be 0.
    uint8_t zero : 1;
    // Global bit, ignored in this kernel, must be 0.
    uint8_t zero2 : 1;
    // This field is ignored by the processor.
    uint8_t ignored : 3;
    // Address of the physical frame. Must be 4KiB aligned.
    uint32_t pageframe_addr : 20;
} __attribute__((packed));

// Pages are 4KiB.
#define PAGE_SIZE   (4096)
// Page Directories must fit within one page.
#define PAGEDIR_ENTRIES_COUNT (PAGE_SIZE/sizeof(struct pagedir_entry_t))
// Page Tables must fit within one page.
#define PAGETABLE_ENTRIES_COUNT (PAGE_SIZE/sizeof(struct pagetable_entry_t))

struct pagedir_t {
    struct pagedir_entry_t entry[PAGEDIR_ENTRIES_COUNT];
} __attribute__((packed));

struct pagetable_t {
    struct pagetable_entry_t entry[PAGETABLE_ENTRIES_COUNT];
} __attribute__((packed));


struct vm {
    // The physical address of the pagedir.
    p_addr pagedir_addr;
};
extern struct vm KERNEL_PAGEDIRECTORY;

// Initialize and enable paging.
void
paging_init(void);

// Check if paging is enabled on the processor by reading the PG bit in CR0.
//   @return True if paging is enabled, false otherwise.
bool
paging_enabled(void);

// Map a physical memory region start:start+size to dest:dest+size. All
// addresses must be PAGE_SIZE aligned. Size must be a multiple of PAGE_SIZE.
// Uses the page directory and frame allocator specified as arguments.
void
paging_map(struct vm * const vm,
           struct frame_alloc_t * const allocator,
           p_addr const start,
           size_t const size,
           v_addr const dest);

// Prints the layout of the pagedirectory of the kernel in the serial console.
void
paging_dump_pagedir(void);

// Short-cut to avoid boiler-plate.
#define paging_create_map(start, size, dest) \
    paging_map(&KERNEL_PAGEDIRECTORY, &FRAME_ALLOCATOR, start, size, dest)
