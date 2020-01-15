#pragma once
#include <types.h>

#define PAGE_SIZE   0x1000

// Initialize paging.
void init_paging(void const * const esp);

#define VM_WRITE            (1 << 0)
#define VM_WRITE_THROUGH    (1 << 1)
#define VM_CACHE_DISABLE    (1 << 2)
#define VM_USER             (1 << 3)
#define VM_NON_GLOBAL       (1 << 4)

void paging_map(void const * const paddr,
                void const * const vaddr,
                size_t const len,
                uint32_t const flags);

void paging_unmap(void const * const vaddr,
                  size_t const len);

// Execute tests related to paging.
void paging_test(void);
