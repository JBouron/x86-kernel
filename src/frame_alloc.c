#include <frame_alloc.h>
#include <bitmap.h>
#include <cpu.h>
#include <math.h>
#include <kernel_map.h>
#include <debug.h>
#include <kernel_map.h>

// The frame allocator allocates frames in the physical range 0x00000000 -
// 0x00E00000.
// We use a bitmap to indicate the state of each physical frame in this range.
// A 1 bit indicate that this frame is currently used ie. not available, a 0 bit
// indicate that this frame is available for use ie. can be allocated.

#define FRAME_ALLOC_SIZE    (0x00E00000 / 0x1000)
DECLARE_BITMAP(FRAME_BITMAP, FRAME_ALLOC_SIZE);

// Get a pointer on the bitmap of the frame allocator.
// @return: Either a physical or virtual pointer whether or not paging has been
// enabled already.
static struct bitmap_t *get_bitmap_addr(void) {
    if (cpu_paging_enabled()) {
        return &FRAME_BITMAP;
    } else {
        return to_phys(&FRAME_BITMAP);
    }
}

void init_frame_alloc(void) {
    // We expect this function to be called before paging is enabled. Note:
    // Assert will probably not work here as the output might not be
    // initialized.
    ASSERT(!cpu_paging_enabled());
    struct bitmap_t * const bitmap = get_bitmap_addr();
    // Since the frame allocator is initialized before paging we need to set the
    // `data` pointer to a physical address before manipulating the bitmap. Once
    // paging is enabled it will be reverted to its virtual address.
    bitmap->data = to_phys(bitmap->data);

    // Reset the bitmap, all the frames are marked as free.
    bitmap_reset(bitmap);

    // Mark frames 0 -> <last frame used by kernel> as non free.
    uint32_t const kernel_end_offset = (uint32_t)to_phys(KERNEL_END_ADDR);
    uint32_t const first_avail_frame = ceil_x_over_y_u32(kernel_end_offset,
        0x1000);
    for (uint32_t i = 0; i < first_avail_frame; ++i) {
        bitmap_set(bitmap, i);
    }
}

void fixup_frame_alloc_to_virt(void) {
    // Paging has been enabled, we can now use a virtual pointer for the `data`
    // field of the bitmap.
    struct bitmap_t * const bitmap = get_bitmap_addr();
    bitmap->data = to_virt(bitmap->data);
}

void *alloc_frame(void) {
    struct bitmap_t * const bitmap = get_bitmap_addr();
    // Try to find the next free frame (that is the next free bit in the
    // bitmap). If no frame is available, panic, there is nothing to do.
    uint32_t const frame_idx = bitmap_set_next_bit(bitmap);
    if (frame_idx == BM_NPOS) {
        PANIC("No physical frame available.");
    }
    // A frame is available, its address is the bit position * PAGE_SIZE.
    void * const frame_addr = (void*)(frame_idx * 0x1000);
    return frame_addr;
}

// Compute the index of the bit in the bitmap corresponding to a physical frame
// address.
// @param ptr: The physical address of the frame of which the function should
// return the index. Note: this address _must_ be 4KiB aligned.
// @return: The index in the bitmap of the frame pointed by `ptr`.
static uint32_t frame_index(void const * const ptr) {
    ASSERT(is_4kib_aligned(ptr));
    return ((uint32_t)ptr) / 0x1000;
}

void free_frame(void const * const ptr) {
    struct bitmap_t * const bitmap = get_bitmap_addr();

    // The pointer is supposed to describe a physical frame and therefore should
    // be 4KiB aligned.
    ASSERT(is_4kib_aligned(ptr));

    // First check that the frame is currently in use.
    uint32_t const idx = frame_index(ptr);
    bool const in_use = bitmap_get_bit(bitmap, idx);

    if (!in_use) {
        // Even though it wouldn't break anything we should panic on a double
        // free as it might be helpful to find a bug in the caller.
        PANIC("Double free");
    }

    // The frame is currently in use, free the bit up.
    bitmap_unset(bitmap, idx);
}

uint32_t frames_allocated(void) {
    struct bitmap_t * const bitmap = get_bitmap_addr();
    return bitmap->size - bitmap->free;
}

#include <frame_alloc.test>
