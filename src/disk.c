#include <disk.h>
#include <math.h>
#include <memory.h>
#include <debug.h>
#include <kmalloc.h>

// Get the index of the sector containing a particular offset on disk.
// @param offset: The offset to convert to sector index.
// @param sec_size: The sector size of the device.
// @return: The index of the sector containing the word at `offset`.
static sector_t offset_to_sector(off_t const offset, size_t const sec_size) {
    return (sector_t)(offset / sec_size);
}

// Get the offset of the first word in a sector.
// @param sec: The index of the sector.
// @param sec_size: THe sector size of the device.
// @return: The offset of the first word of the sector with index == `sec`.
static off_t sector_to_offset(sector_t const sec, size_t const sec_size) {
    return (off_t)(sec * sec_size);
}

size_t disk_read(struct disk * const disk,
                 off_t const offset,
                 uint8_t * const buf,
                 size_t const len) {
    uint32_t const sector_size = disk->ops->sector_size(disk);
    sector_t const start_sector = offset_to_sector(offset, sector_size);
    sector_t const end_sector = offset_to_sector(offset + len - 1, sector_size);

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
            // This is the first sector, the offset to read from might point
            // to the middle of this sector.
            ASSERT(sector_to_offset(sector, sector_size) <= offset);
            start_offset = offset - sector_to_offset(sector, sector_size);
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
                  off_t const offset,
                  uint8_t const * const buf,
                  size_t const len) {
    if (!len) {
        // Empty write.
        return 0;
    }

    uint32_t const sector_size = disk->ops->sector_size(disk);
    sector_t const start_sector = offset_to_sector(offset, sector_size);
    sector_t const end_sector = offset_to_sector(offset + len - 1, sector_size);

    // The number of bytes written onto disk.
    uint32_t written = 0;

    for (sector_t sector = start_sector; sector <= end_sector; ++sector) {
        if ((sector == start_sector && offset % sector_size != 0) ||
            (sector == end_sector && (offset + len) % sector_size != 0)) {
            // The first and last sectors might need a partial update, which is
            // only possible by reading the sector and writting it back again.

            // Read the current data on the sector.
            uint8_t * const curr = kmalloc(sector_size);
            uint32_t const read = disk->ops->read_sector(disk, sector, curr);
            if (read != sector_size) {
                // We failed to read the sector to update it, this could happen
                // if the write tries to write outside the boundaries of the
                // disk. Just abort now.
                kfree(curr);
                break;
            }

            // Compute the offset and length to use when moving data from the
            // buffer to `curr`.
            off_t const sec_start_off = sector_to_offset(sector, sector_size);
            off_t const next_start_off = sector_to_offset(sector + 1,
                                                          sector_size);
            uint32_t const c_off = offset + written - sec_start_off;
            uint32_t const cpy_len =
                min_u32(next_start_off - (sec_start_off+c_off), len - written);

            // Update the `curr`.
            memcpy(curr + c_off, buf + written, cpy_len);

            // Write back the updated sector onto the disk.
            uint32_t const res = disk->ops->write_sector(disk, sector, curr);

            kfree(curr);

            // The "effective" amount of written bytes is the number of bytes
            // that were overwritten in the current sector NOT the returned
            // value from write_sector since we may have updated the sector
            // partially.
            written += cpy_len;
            if (res != sector_size) {
                // Having an error in the middle of the write should abort.
                break;
            }
        } else {
            // All sectors between the first and last will receive a full
            // update.
            uint8_t const * const data = buf + written;
            ASSERT(data < buf + len);
            ASSERT(data + sector_size <= buf + len);
            uint32_t const res = disk->ops->write_sector(disk, sector, data);
            written += res;
            if (res != sector_size) {
                // Having an error in the middle of the write should abort.
                break;
            }
        }
    }
    return written;
}

#include <disk.test>
