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

static struct bitmap_t *get_bitmap_addr(struct bitmap_t * const ptr) {
    if (cpu_paging_enabled()) {
        return ptr;
    } else {
        return to_phys(ptr);
    }
}

void init_frame_alloc(void) {
    // We expect this function to be called before paging is enabled. Note:
    // Assert will probably not work here as the output might not be
    // initialized.
    ASSERT(!cpu_paging_enabled());
    struct bitmap_t * const bitmap = get_bitmap_addr(&FRAME_BITMAP);
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
    struct bitmap_t * const bitmap = get_bitmap_addr(&FRAME_BITMAP);
    bitmap->data = to_virt(bitmap->data);
}

void *alloc_frame(void) {
    struct bitmap_t * const bitmap = get_bitmap_addr(&FRAME_BITMAP);
    uint32_t const frame_idx = bitmap_set_next_bit(bitmap);
    void * const frame_addr = (void*)(frame_idx * 0x1000);
    return frame_addr;
}
