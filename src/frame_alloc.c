#include <frame_alloc.h>
#include <bitmap.h>
#include <cpu.h>
#include <math.h>
#include <kernel_map.h>
#include <debug.h>
#include <kernel_map.h>
#include <paging.h>
#include <multiboot.h>
#include <spinlock.h>
#include <error.h>
#include <memory.h>

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

// Acquire the frame allocator lock and return a pointer on the frame
// allocator's bitmap.
// @return: The virtual address of the frame allocator's bitmap.
static struct bitmap *get_bitmap_and_lock(void) {
    spinlock_lock(&FRAME_ALLOC_LOCK);
    return &FRAME_BITMAP;
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
    uint32_t const start_frame = frame_index(start);
    uint32_t const end_frame = frame_index(end);
    LOG("  %p - %p (%u frames)\n", start, end, end_frame - start_frame + 1);
    for (uint32_t i = start_frame; i <= end_frame; ++i) {
        ASSERT(!bitmap_get_bit(&FRAME_BITMAP, i));
        bitmap_set(&FRAME_BITMAP, i);
    }
}

static void unmark_memory_range(void const * const start,
                                void const * const end) {
    uint32_t const start_frame = frame_index(start);
    uint32_t const end_frame = frame_index(end);
    LOG("  %p - %p (%u frames)\n", start, end, end_frame - start_frame + 1);
    for (uint32_t i = start_frame; i <= end_frame; ++i) {
        ASSERT(bitmap_get_bit(&FRAME_BITMAP, i));
        bitmap_unset(&FRAME_BITMAP, i);
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

    struct multiboot_mmap_entry const * ptr;
    for (ptr = first; ptr < first + count; ++ptr) {
        struct multiboot_mmap_entry entry;
        phy_read(ptr, &entry, sizeof(entry));
        if (mmap_entry_is_available(&entry) && mmap_entry_within_4GiB(&entry)) {
            void const * const start = (void*)(uint32_t)entry.base_addr;
            void const * const end = get_max_addr_for_entry(&entry);
            unmark_memory_range(start, end);
        }
    }
}

// Mark the frame(s) containing the multiboot structure as allocated in the
// bitmap.
static void mark_multiboot_info_struct(void) {
    struct multiboot_info const * const mb_struct =
        to_phys(get_multiboot_info_struct());

    void const * const mb_start = mb_struct;
    void const * const mb_end = mb_start + sizeof(*mb_struct) - 1;
    mark_memory_range(mb_start, mb_end);
}

// Mark all physical frames on which the init ram-disk was loaded as not
// available.
static void mark_initrd_frames(void) {
    // Mark all the frames used by the data coming from the initrd.
    size_t const initrd_size = multiboot_get_initrd_size();
    if (!initrd_size) {
        LOG("No initrd found.\n");
        return;
    }

    void const * const initrd_start = multiboot_get_initrd_start();
    void const * const initrd_end = initrd_start + initrd_size - 1;
    mark_memory_range(initrd_start, initrd_end);
}

void init_frame_alloc(void) {
    LOG("Initializing Physical Frame Allocator (PFA).\n");

    // We expect this function to be called before paging is enabled but after
    // GDT is enabled and we made the initial jump to higher half using it.
    ASSERT(!cpu_paging_enabled());
    ASSERT(in_higher_half());

    // Executing this function without acquiring the FRAME_ALLOC_LOCK is safe as
    // we are doing this very early in the BSP boot sequence, hence APs are not
    // woken up yet.

    struct bitmap *const bitmap = &FRAME_BITMAP;

    // First compute the number of bits required to keep track of the state of
    // _all_ the frames in RAM.
    uint32_t const bitmap_size = compute_bitmap_size();
    LOG("PFA will use a bitmap of %u bits to track frames.\n", bitmap_size);

    // We are going to store this potentially huge bitmap in RAM. Do do so, we
    // need to find a continuous memory range big enough to fit all the bits.
    // Because we will need to access this bitmap from the virtual address space
    // we use a page size granularity.
    uint32_t const num_frames = ceil_x_over_y_u32(bitmap_size, (8 * 0x1000));
    LOG("PFA's bitmap will be stored on %u physical frames.\n", num_frames);

    // Save the number of frames allocated for the bitmap, this will be useful
    // for mapping the bitmap to virtual address space.
    NUM_FRAMES_FOR_BITMAP = num_frames;

    // Try to find `num_frames` contiguous availables physical frames in RAM by
    // looking at the memory map.
    void * const start_frame_phy = find_contiguous_physical_frames(num_frames);
    ASSERT(is_4kib_aligned(start_frame_phy));
    LOG("PFA's bitmap stored at physical address %p.\n", start_frame_phy);

    // We need to store the virtual address of the start frame in the bitmap in
    // order to use it.
    void * const start_frame = to_virt(start_frame_phy);

    // Initialize the bitmap. Mark _all_ the frames as allocated.
    bitmap_init(bitmap, bitmap_size, (uint32_t*)start_frame, true);

    // Now go over the memory map and mark all the frames that are available to
    // us as free.
    LOG("Unmarking all available (per multiboot header) physical frames:\n");
    unmark_avail_frames();

    // Because unmark_avail_frames will also unmark frames that are currently
    // holding important data such as the kernel itself but also the bitmap and
    // the multiboot data structure, we need to make a pass over those and mark
    // them as allocated so that the allocator doesn't hand them out.

    // Mark the pages used by the kernel as allocated.
    LOG("Marking all frames used by kernel:\n");
    mark_kernel_frames();

    // Mark the frames used for the bitmap.
    LOG("Marking all frames used to store the bitmap:\n");
    mark_bitmap_frames(start_frame_phy, num_frames);

    // Mark any page used to store the multiboot info structure. That way we
    // don't end up over writing it.
    LOG("Marking all frames used to store the multiboot header:\n");
    mark_multiboot_info_struct();

    // Mark the physical frames used by initrd.
    LOG("Marking all frames used to store the initrd:\n");
    mark_initrd_frames();

    // The frame allocator is ready to roll.
    LOG("PFA initialized:\n");
    LOG("  .size = %u\n", FRAME_BITMAP.size);
    LOG("  .free = %u\n", FRAME_BITMAP.free);
    LOG("  .data = %p\n", FRAME_BITMAP.data);

    // If the data of the frame allocator is above kernel addresses, then once
    // we enable paging we will need to manually map those frames to virtual
    // address space. For now, the bitmap always fit under the 1MiB limit which
    // always gets mapped with the kernel in higher half.
    ASSERT((void*)FRAME_BITMAP.data < (void*)KERNEL_START_ADDR);
}

// The maximum index for memory under 1MiB.
#define LOW_MEM_MAX_IDX   (((1 << 20) / PAGE_SIZE) - 1)

// Perform the actual frame allocation.
// @param low_mem: If true, this function will try to allocate a frame under the
// 1MiB limit. Otherwise it will try anywhere in physical memory.
static void *do_allocation(bool const low_mem) {
    struct bitmap * const bitmap = get_bitmap_and_lock();

    if (OOM_SIMULATION) {
        spinlock_unlock(&FRAME_ALLOC_LOCK);
        SET_ERROR("OOM Simulation active", ENOMEM);
        return NO_FRAME;
    }

    void * frame_addr;

    uint32_t const start_idx = low_mem ? 0 : LOW_MEM_MAX_IDX; 
    uint32_t frame_idx = bitmap_set_next_bit(bitmap, start_idx);

    if (frame_idx == BM_NPOS) {
        if (!low_mem) {
            // For non-low mem request we have another chance by looking at the
            // low memory frames.
            frame_idx = bitmap_set_next_bit(bitmap, 0);
        }
    } else if (low_mem) {
        // Make sure that the frame is under 1MiB.
        if (frame_idx > LOW_MEM_MAX_IDX) {
            // No available frames under 1MiB. Revert.
            bitmap_unset(bitmap, frame_idx);
            frame_idx = BM_NPOS;
        }
    }

    if (frame_idx == BM_NPOS) {
        SET_ERROR("No physical frame left for allocation", ENOMEM);
        frame_addr = NO_FRAME;
    } else {
        // A frame is available, its address is the bit position * PAGE_SIZE.
        frame_addr = (void*)(frame_idx * 0x1000);
    }

    spinlock_unlock(&FRAME_ALLOC_LOCK);
    return frame_addr;
}

void *alloc_frame(void) {
    return do_allocation(false);
}

void *alloc_frame_low_mem(void) {
    return do_allocation(true);
}

void free_frame(void const * const ptr) {
    ASSERT(ptr != NO_FRAME);
    struct bitmap * const bitmap = get_bitmap_and_lock();

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
    struct bitmap * const bitmap = get_bitmap_and_lock();
    uint32_t const n_allocs = bitmap->size - bitmap->free;
    spinlock_unlock(&FRAME_ALLOC_LOCK);
    return n_allocs;
}

void frame_alloc_set_oom_simulation(bool const enabled) {
    OOM_SIMULATION = enabled;
}

#include <frame_alloc.test>
