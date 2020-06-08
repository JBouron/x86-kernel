#include <initrd.h>
#include <multiboot.h>
#include <paging.h>
#include <debug.h>
#include <math.h>
#include <kernel_map.h>
#include <memory.h>
#include <disk.h>

// Since we assume there can be a single initrd loaded with the kernel, it
// suffices to used static variables.

// The address where the initrd has been mapped in virtual RAM.
static void const * INITRD_ADDR = NULL;

// The size of the initrd in bytes.
static size_t INITRD_SIZE = 0;

// The sector size used by the initrd. This is arbitrary as the initrd is not
// sector based as disk devices.
static uint32_t const INITRD_SECTOR_SIZE = 512;

// The struct disk associated with the initrd.
static struct disk INITRD_DISK;

// Get the sector size used by the struct disk for the initrd.
// @param disk: The disk.
// @return: The sector size.
static uint32_t initrd_sector_size(struct disk * const disk) {
    // There can only be one initrd.
    ASSERT(disk == &INITRD_DISK);
    return INITRD_SECTOR_SIZE;
}

// Read a sector from the initrd ""disk"".
// @param disk: The disk.
// @param sector_index: The index of the sector to be read.
// @param buf: The buffer to read the sector into. This buffer must be at least
// of size INITRD_SECTOR_SIZE.
static uint32_t initrd_read_sector(struct disk * const disk,
                                   sector_t const sector_index,
                                   uint8_t * const buf) {
    // There can only be one initrd.
    ASSERT(disk == &INITRD_DISK);
    sector_t const last_sector = ceil_x_over_y_u32(INITRD_SIZE,
                                                   INITRD_SECTOR_SIZE);

    if (sector_index > last_sector) {
        // Reached the end of the initrd.
        return 0;
    }

    uint32_t const offset = sector_index * INITRD_SECTOR_SIZE;
    size_t const len = min_u32(INITRD_SECTOR_SIZE, INITRD_SIZE - offset);

    if (len < INITRD_SECTOR_SIZE) {
        // Since the initrd has a size with a byte granularity, we might have
        // less than one sector of data left to read. If that is the case, pad
        // with zeros.
        memzero(buf, INITRD_SECTOR_SIZE);
    }

    memcpy(buf, INITRD_ADDR + offset, len);
    return INITRD_SECTOR_SIZE;
}

// Write a sector to the initrd ""disk"".
// @param disk: The disk.
// @param sector_index: The index of the sector to be written.
// @param buf: The buffer to write into the sector. This buffer must be of size
// INITRD_SECTOR_SIZE.
// Note: Initrd are by definition read only, this function will panic if called.
static uint32_t initrd_write_sector(struct disk * const disk,
                                    sector_t const sector_index,
                                    uint8_t const * const buf) {
    PANIC("Cannot write on initrd.\n");
    // Avoid warning.
    return 0;
}

// The disk ops for the initrd disk.
static struct disk_ops const initrd_ops = {
    .sector_size = initrd_sector_size,
    .read_sector = initrd_read_sector,
    .write_sector = initrd_write_sector,
};

void init_initrd_driver(void) {
    void * const phy_addr = multiboot_get_initrd_start();
    INITRD_SIZE = multiboot_get_initrd_size();

    // We expect the bootloader to map the initrd on a 4KiB aligned page.
    ASSERT(is_4kib_aligned(phy_addr));
    size_t const num_frames = ceil_x_over_y_u32(INITRD_SIZE, PAGE_SIZE);
    ASSERT(num_frames);

    // Map the initrd into the kernel address space.
    void * frames[num_frames];
    for (size_t i = 0; i < num_frames; ++i) {
        frames[i] = phy_addr + i * PAGE_SIZE;
    }
    INITRD_ADDR = paging_map_frames_above(KERNEL_PHY_OFFSET, 
                                          frames,
                                          num_frames,
                                          0);

    INITRD_DISK.ops = &initrd_ops;
    // No data needed.
    INITRD_DISK.data = NULL;
}

struct disk *get_initrd_disk(void) {
    return &INITRD_DISK;
}
