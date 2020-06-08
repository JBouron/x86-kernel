#include <disk.h>
#include <math.h>
#include <memory.h>
#include <debug.h>
#include <kmalloc.h>

// Get the index of the sector containing a particular address on disk.
// @param addr: The address to convert to sector index.
// @param sec_size: The sector size of the device.
// @return: The index of the sector containing the word at `addr`.
static sector_t addr_to_sector(void const * const addr, size_t const sec_size) {
    return (sector_t)((uint32_t)(void*)addr / sec_size);
}

// Get the address of the first word in a sector.
// @param sec: The index of the sector.
// @param sec_size: THe sector size of the device.
// @return: The address of the first word of the sector with index == `sec`.
static void *sector_to_addr(sector_t const sec, size_t const sec_size) {
    return (void*)(uint32_t)(sec * sec_size);
}

size_t disk_read(struct disk * const disk,
                 void const * const addr,
                 uint8_t * const buf,
                 size_t const len) {
    uint32_t const sector_size = disk->ops->sector_size(disk);
    sector_t const start_sector = addr_to_sector(addr, sector_size);
    sector_t const end_sector = addr_to_sector(addr + len - 1, sector_size);

    // The number of bytes written to the buffer `buf` so far.
    uint32_t written = 0;

    // All sectors will be read into the `sector_data` buffer. FIXME: This could
    // be optimized to read into the buffer `buf` instead.
    uint8_t * const sector_data = kmalloc(sector_size);

    for (sector_t sector = start_sector; sector <= end_sector; ++sector) {
        memzero(sector_data, sector_size);

        uint32_t const read = disk->ops->read_sector(disk, sector, sector_data);
        if (read == 0) {
            // We reached the end of the disk, no more to read.
            break;
        }

        // The read_sector() function should return a complete sector or
        // nothing.
        ASSERT(read == sector_size);

        uint32_t start_offset = 0;
        if (sector == start_sector) {
            // This is the first sector, the address to read from might point
            // to the middle of this sector.
            ASSERT(sector_to_addr(sector, sector_size) <= addr);
            start_offset = addr - sector_to_addr(sector, sector_size);
        } else {
            // For all sectors after the fist one copy from the start of the
            // sector.
            start_offset = 0;
        }

        ASSERT(start_offset < sector_size);
        ASSERT(written <= len);

        uint32_t const remaining = len - written;
        size_t const copy_len = min_u32(remaining, sector_size - start_offset);
        memcpy(buf + written, sector_data + start_offset, copy_len);
        written += copy_len;
    }
    kfree(sector_data);
    return written;
}

size_t disk_write(struct disk * const disk,
                  void const * const addr,
                  uint8_t const * const buf,
                  size_t const len) {
    // TODO: Implement write.
    return 0;
}

#include <disk.test>
