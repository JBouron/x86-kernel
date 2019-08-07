#include <memory/paging/alloc/early_boot.h>
#include <utils/kernel_map.h>
#include <utils/memory.h>

#define PAGE_SIZE (4096)
#define KERNEL_VIRT_START_ADDR  (0xC0000000)
#define PHYS_ADDR(addr) ((p_addr)(((uint8_t*)(addr)) - KERNEL_VIRT_START_ADDR))

void
fa_early_boot_init(struct early_boot_frame_alloc_t * const allocator) {
    // Setup the function pointer to the allocating function. Note that we need
    // its physical address since at this point paging is still disabled.
    allocator->alloc_frame = (p_addr (*)(struct frame_alloc_t *const))
        PHYS_ADDR(fa_early_boot_alloc_frame);
    // The first allocatable address is the first 4KiB aligned address after the
    // kernel. The memory range KERNEL-END to 0x00EFFFFF is guaranteed to be
    // available to us.
    p_addr const kernel_end = PHYS_ADDR(KERNEL_END);
    // TODO: The page size should be a constant, but we don't want to include
    // paging.h here.
    allocator->next_frame_addr = (1 + kernel_end / PAGE_SIZE) * PAGE_SIZE;
}

p_addr
fa_early_boot_alloc_frame(struct frame_alloc_t * const allocator) {
    // Convert the allocator to an early_boot_allocator.
    struct early_boot_frame_alloc_t * const eb_allocator =
        (struct early_boot_frame_alloc_t *)allocator;

    // The address pointed by next_frame_addr is free, so use it and increase
    // the next_frame_addr pointer by PAGE_SIZE;
    p_addr const addr = eb_allocator->next_frame_addr;
    // TODO: The page size should be a constant, but we don't want to include
    // paging.h here.
    eb_allocator->next_frame_addr += PAGE_SIZE;

    // Zero the freshly allocated frame before returning it.
    memzero((uint8_t*)addr, PAGE_SIZE);
    return addr;
}
