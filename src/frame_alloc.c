#include <frame_alloc.h>
#include <bitmap.h>
#include <cpu.h>
#include <math.h>
#include <kernel_map.h>
#include <debug.h>
#include <kernel_map.h>
#include <paging.h>
#include <multiboot.h>
#include <lock.h>

// There is a single frame allocator for the whole system. Hence we need a lock
// to avoid race conditions.
DECLARE_SPINLOCK(FRAME_ALLOC_LOCK);

// We use a bitmap to indicate the state of each physical frame in this range.
// A 1 bit indicate that this frame is currently used ie. not available, a 0 bit
// indicate that this frame is available for use ie. can be allocated.
struct bitmap FRAME_BITMAP;

// The number of physical frames used for the bitmap.
static uint32_t NUM_FRAMES_FOR_BITMAP = 0;

// Out-Of-Memory simulation flag.
static bool OOM_SIMULATION = false;

// Get a pointer on the bitmap of the frame allocator.
// @return: Either a physical or virtual pointer whether or not paging has been
// enabled already.
static struct bitmap *get_bitmap_addr(void) {
    if (cpu_paging_enabled()) {
        spinlock_lock(&FRAME_ALLOC_LOCK);
        return &FRAME_BITMAP;
    } else {
        return to_phys(&FRAME_BITMAP);
    }
}

// Compute the size of the bitmap required to keep track of all the physical
// frames in RAM.
// @return: The necessary number of bits in the bitmap to address all frames.
static uint32_t compute_bitmap_size(void) {
    // Since we use one bit per physical frame, we need <MAX ADDR> / PAGE SIZE
    // bits total (ceiling). For a full 4GiB of RAM this is 2^20 bits that is
    // 128KiB of RAM.
    return ceil_x_over_y_u32((uint32_t)get_max_addr(), 0x1000);
}

// Compute the index of the bit in the bitmap corresponding to a physical 
// address.
// @param ptr: The physical address of the frame of which the function should
// return the index.
// @return: The index in the bitmap of the frame pointed by `ptr`.
static uint32_t frame_index(void const * const ptr) {
    return ((uint32_t)get_page_addr(ptr)) >> 12;
}

static void mark_memory_range(void const * const start,
                              void const * const end) {
    struct bitmap * const bitmap = get_bitmap_addr();
    uint32_t const start_frame = frame_index(start);
    uint32_t const end_frame = frame_index(end);
    for (uint32_t i = start_frame; i <= end_frame; ++i) {
        ASSERT(!bitmap_get_bit(bitmap, i));
        bitmap_set(bitmap, i);
    }
}

static void unmark_memory_range(void const * const start,
                                void const * const end) {
    struct bitmap * const bitmap = get_bitmap_addr();
    uint32_t const start_frame = frame_index(start);
    uint32_t const end_frame = frame_index(end);
    for (uint32_t i = start_frame; i <= end_frame; ++i) {
        ASSERT(bitmap_get_bit(bitmap, i));
        bitmap_unset(bitmap, i);
    }
}

// Mark every physical frames that contains the kernel as allocated in the
// bitmap.
static void mark_kernel_frames(void) {
    void const * const kstart = to_phys(KERNEL_START_ADDR);
    void const * const kend = to_phys(KERNEL_END_ADDR);
    mark_memory_range(kstart, kend);
}

// Mark every physical frames that contains the actual bitmap as allocated.
static void mark_bitmap_frames(void const * const start, uint32_t const n) {
    void const * const end = start + (n * 0x1000) - 1;
    mark_memory_range(start, end);
}

// Go over the memroy map and mark all the frames that are available as
// non-allocated.
static void unmark_avail_frames(void) {
    struct multiboot_mmap_entry const * const first = get_mmap_entry_ptr();
    uint32_t const count = multiboot_mmap_entries_count();

    struct multiboot_mmap_entry const * entry = NULL;
    for (entry = first; entry < first + count; ++entry) {
        if (mmap_entry_is_available(entry) && mmap_entry_within_4GiB(entry)) {
            void const * const start = (void*)(uint32_t)entry->base_addr;
            void const * const end = get_max_addr_for_entry(entry);
            unmark_memory_range(start, end);
        }
    }
}

// Mark the frame(s) containing the multiboot structure as allocated in the
// bitmap.
static void mark_multiboot_info_struct(void) {
    struct multiboot_info const * const mb_struct = get_multiboot_info_struct();
    void const * const mb_start = mb_struct;
    void const * const mb_end = mb_start + sizeof(*mb_struct) - 1;
    mark_memory_range(mb_start, mb_end);
}

// Mark all physical frames on which the init ram-disk was loaded as not
// available.
static void mark_initrd_frames(void) {
    // Mark all the frames used by the data coming from the initrd.
    void const * const initrd_start = multiboot_get_initrd_start();
    size_t const initrd_size = multiboot_get_initrd_size();
    void const * const initrd_end = initrd_start + initrd_size - 1;
    mark_memory_range(initrd_start, initrd_end);
}

void init_frame_alloc(void) {
    // We expect this function to be called before paging is enabled. Note:
    // Assert will probably not work here as the output might not be
    // initialized.
    ASSERT(!cpu_paging_enabled());

    // Executing this function without acquiring the FRAME_ALLOC_LOCK is safe as
    // we are doing this very early in the BSP boot sequence, hence APs are not
    // woken up yet.
    struct bitmap * const bitmap = get_bitmap_addr();

    // First compute the number of bits required to keep track of the state of
    // _all_ the frames in RAM.
    uint32_t const bitmap_size = compute_bitmap_size();

    // We are going to store this potentially huge bitmap in RAM. Do do so, we
    // need to find a continuous memory range big enough to fit all the bits.
    // Because we will need to access this bitmap from the virtual address space
    // we use a page size granularity.
    uint32_t const num_frames = ceil_x_over_y_u32(bitmap_size, (8 * 0x1000));

    // Save the number of frames allocated for the bitmap, this will be useful
    // for mapping the bitmap to virtual address space.
    *((uint32_t*)to_phys(&NUM_FRAMES_FOR_BITMAP)) = num_frames;

    // Try to find `num_frames` contiguous availables physical frames in RAM by
    // looking at the memory map.
    void * const start_frame = find_contiguous_physical_frames(num_frames);
    ASSERT(is_4kib_aligned(start_frame));

    // The following check is a regression test that the bitmap will not
    // overwrite the multiboot struct.
    void const * const mb = (void*)get_multiboot_info_struct();
    ASSERT(start_frame > mb || (start_frame + (num_frames * 0x1000) - 1 < mb));

    // Initialize the bitmap. Mark _all_ the frames as allocated.
    bitmap_init(bitmap, bitmap_size, (uint32_t*)start_frame, true);

    // Now go over the memory map and mark all the frames that are available to
    // us as free.
    unmark_avail_frames();

    // Because unmark_avail_frames will also unmark frames that are currently
    // holding important data such as the kernel itself but also the bitmap and
    // the multiboot data structure, we need to make a pass over those and mark
    // them as allocated so that the allocator doesn't hand them out.

    // Mark the pages used by the kernel as allocated.
    mark_kernel_frames();

    // Mark the frames used for the bitmap.
    mark_bitmap_frames(start_frame, num_frames);

    // Mark any page used to store the multiboot info structure. That way we
    // don't end up over writing it.
    mark_multiboot_info_struct();

    // Mark the physical frames used by initrd.
    mark_initrd_frames();

    // The frame allocator is ready to roll.
}

void fixup_frame_alloc_to_virt(void) {
    // Paging has been enabled, we can now use a virtual pointer for the `data`
    // field of the bitmap.
    struct bitmap * const bitmap = get_bitmap_addr();

    // APs are still not woken up, no need to acquire FRAME_ALLOC_LOCK.

    // Save the physical address of the bitmap data.
    void const * const frame_addr = bitmap->data;
    void * const virt_addr = to_virt(bitmap->data);

    // It is important to create the mapping BEFORE setting the data pointer to
    // the virtual address as paging_map will inevitably call to alloc_frame.
    // This works because the identity mapping is still present therefore, while
    // allocating for the mapping, the alloc_call will use the old physical
    // pointer to the data.
    if (virt_addr > (void*)KERNEL_PHY_OFFSET + 0x100000) {
        // The 1MiB below the kernel has already been mapped to higher half,
        // hence there is no need to map it again.
        // This is actually unlikely since the bitmap will pretty much always
        // fit in the available memory under 1MiB. Nevertheless, in case it
        // doesn't, we need to be careful.
        paging_map(frame_addr, virt_addr, NUM_FRAMES_FOR_BITMAP, VM_WRITE);
    }

    // Use the virtual address of the bitmap for now on.
    bitmap->data = virt_addr;
    spinlock_unlock(&FRAME_ALLOC_LOCK);
}

void *alloc_frame(void) {
    struct bitmap * const bitmap = get_bitmap_addr();

    if (OOM_SIMULATION) {
        spinlock_unlock(&FRAME_ALLOC_LOCK);
        return NO_FRAME;
    }

    // Try to find the next free frame (that is the next free bit in the
    // bitmap). If no frame is available, panic, there is nothing to do.
    uint32_t const frame_idx = bitmap_set_next_bit(bitmap);
    void * frame_addr;
    if (frame_idx == BM_NPOS) {
        LOG("No physical frame left for allocation.\n");
        frame_addr = NO_FRAME;
    } else {
        // A frame is available, its address is the bit position * PAGE_SIZE.
        frame_addr = (void*)(frame_idx * 0x1000);
    }
    spinlock_unlock(&FRAME_ALLOC_LOCK);
    return frame_addr;
}

void free_frame(void const * const ptr) {
    ASSERT(ptr != NO_FRAME);
    struct bitmap * const bitmap = get_bitmap_addr();

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
    spinlock_unlock(&FRAME_ALLOC_LOCK);
}

uint32_t frames_allocated(void) {
    struct bitmap * const bitmap = get_bitmap_addr();
    uint32_t const n_allocs = bitmap->size - bitmap->free;
    spinlock_unlock(&FRAME_ALLOC_LOCK);
    return n_allocs;
}

void frame_alloc_set_oom_simulation(bool const enabled) {
    OOM_SIMULATION = enabled;
}

#include <frame_alloc.test>
