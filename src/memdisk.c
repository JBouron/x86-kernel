#include <paging.h>
#include <debug.h>
#include <math.h>
#include <kernel_map.h>
#include <memory.h>
#include <disk.h>
#include <kmalloc.h>

// For now use a sector size of 512 bytes. This is pretty much standard at this
// point.
#define MEMDISK_SEC_SIZE    512

// The private state of a memdisk.
struct memdisk_data {
    // The address where the "content" of the disk is mapped in memory.
    void * mapped_addr;
    // The size of the disk in bytes.
    size_t size;
    // Is the memdisk read only ?
    bool read_only;
};

// Get the private data of a memdisk.
#define get_data(disk)  ((struct memdisk_data*)(disk)->driver_private)

// Get the sector size for a memdisk.
// @param disk: The disk.
// @return: The sector size.
static uint32_t memdisk_sector_size(struct disk * const disk) {
    return MEMDISK_SEC_SIZE;
}

// Read or write a sector from/to a memdisk.
// @param disk: The disk.
// @param sector_index: The index of the sector to be read.
// @param buf: The buffer to read the sector into. The buffer must be of size >=
// the sector size used by the memdisk.
// @param is_read: If true then this function will read the sector into buf, if
// false, the content of buf will be written into the sector.
// @return: The number of bytes successfully read/written, this is either 0 or
// sector size.
static uint32_t do_update(struct disk * const disk,
                          sector_t const sector_index,
                          uint8_t * const buf,
                          bool const is_read) {
    struct memdisk_data const * const data = get_data(disk);
    ASSERT(data);

    if (!is_read && data->read_only) {
        // Forbid writting to a read only memdisk.
        return 0;
    }

    size_t const sector_size = MEMDISK_SEC_SIZE;
    size_t const size = data->size;

    // The sector after the last valid one.
    sector_t const end_sector = ceil_x_over_y_u32(size, sector_size);

    if (sector_index >= end_sector) {
        // Reached the end of the memdisk.
        return 0;
    }

    uint32_t const offset = sector_index * sector_size;
    size_t const len = min_u32(sector_size, size - offset);

    if (len < sector_size && is_read) {
        // Since the memdisk has a size with a byte granularity, we might have
        // less than one sector of data left to read. If that is the case, pad
        // with zeros.
        memzero(buf, sector_size);
    }

    if (is_read) {
        memcpy(buf, data->mapped_addr + offset, len);
    } else {
        memcpy(data->mapped_addr + offset, buf, len);
    }
    return sector_size;
}

// Read a sector from a memdisk.
// @param disk: The disk.
// @param sector_index: The index of the sector to be read.
// @param buf: The buffer to read the sector into. The buffer must be big enough
// to contain at least one sector of the memdisk.
// @return: The number of bytes successfully read, this is either 0 or sector
// size.
static uint32_t memdisk_read_sector(struct disk * const disk,
                                    sector_t const sector_index,
                                    uint8_t * const buf) {
    return do_update(disk, sector_index, buf, true);
}

// Write a sector to the memdisk.
// @param disk: The disk.
// @param sector_index: The index of the sector to be written.
// @param buf: The buffer to write into the sector. This buffer must be of size
// sector_size.
// @return: The number of bytes successfully written, this is either 0 or sector
// size.
static uint32_t memdisk_write_sector(struct disk * const disk,
                                     sector_t const sector_index,
                                     uint8_t const * const buf) {
    return do_update(disk, sector_index, (uint8_t*)buf, false);
}

// The available operations on a memdisk.
struct disk_ops const memdisk_ops = {
    .sector_size = memdisk_sector_size,
    .read_sector = memdisk_read_sector,
    .write_sector = memdisk_write_sector,
};

struct disk *create_memdisk(void * const addr,
                            size_t const size,
                            bool const read_only) {
    if (size % MEMDISK_SEC_SIZE) {
        // Only allow sizes that are multiple of sectors. This simplify a lots
        // of things.
        PANIC("Memdisks must have a size that is a multiple of 512 bytes\n");
    }

    struct disk * const disk = kmalloc(sizeof(*disk));
    disk->ops = &memdisk_ops;

    // Allocate a memdisk_data for this new disk containing its state.
    struct memdisk_data * const data = kmalloc(sizeof(*data));
    disk->driver_private = data;

    data->mapped_addr = addr;
    data->size = size;
    data->read_only = read_only;

    return disk;
}

void delete_memdisk(struct disk * const disk) {
    struct memdisk_data * const data = get_data(disk);
    ASSERT(data);
    kfree(data);
    kfree(disk);
}

#include <memdisk.test>
