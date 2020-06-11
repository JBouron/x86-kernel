#include <initrd.h>
#include <multiboot.h>
#include <paging.h>
#include <debug.h>
#include <math.h>
#include <kernel_map.h>
#include <memory.h>
#include <disk.h>
#include <memdisk.h>

// Since this kernel assumes a single initrd, we can save the memdisk of the
// initrd in a static var. Subsequent calls to get_initrd_disk() will return
// this disk instead of creating a new one.
static struct disk * INIT_RD_MEMDISK = NULL;

// Lock to protect the INIT_RD_MEMDISK variable.
static DECLARE_SPINLOCK(INIT_RD_GET_DISK_LOCK);

struct disk *get_initrd_disk(void) {
    struct disk * disk = NULL;
    spinlock_lock(&INIT_RD_GET_DISK_LOCK);

    if (!INIT_RD_MEMDISK) {
        // We support only a single initrd detected when parsing the multiboot
        // header.
        void * const phy_addr = multiboot_get_initrd_start();
        // The size of a memdisk must be a multiple of 512 bytes. Here this is
        // ok to round up because:
        //  - The physical frames used by the initrd image are only used by that
        //  image. That is, there are nothing between the last byte of the
        //  initrd image and the next frame boundary.
        //  - If the initrd size is a multiple of PAGE_SIZE then it is also a
        //  multiple of 512.
        size_t const size = round_up_u32(multiboot_get_initrd_size(), 512);

        if (!phy_addr) {
            // No initrd was detected when parsing the multiboot header.
            return NULL;
        }

        // We expect the bootloader to map the initrd on a 4KiB aligned page.
        ASSERT(is_4kib_aligned(phy_addr));
        size_t const num_frames = ceil_x_over_y_u32(size, PAGE_SIZE);
        ASSERT(num_frames);

        // Map the initrd into the kernel address space.
        void * frames[num_frames];
        for (size_t i = 0; i < num_frames; ++i) {
            frames[i] = phy_addr + i * PAGE_SIZE;
        }

        // Map the initrd to kernel memory.
        void * const initrd_vaddr = paging_map_frames_above(KERNEL_PHY_OFFSET, 
                                                            frames,
                                                            num_frames,
                                                            0);
        INIT_RD_MEMDISK = create_memdisk(initrd_vaddr, size, true);
    }
    disk = INIT_RD_MEMDISK;

    spinlock_unlock(&INIT_RD_GET_DISK_LOCK);
    return disk;
}
