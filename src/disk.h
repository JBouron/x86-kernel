#pragma once
#include <types.h>

// This file defines generic functions for disk devices.
// Two structures are at play when reading from a disk device:
//   - struct disk: This struct contains all the state of a disk device.
//   Therefore there is one struct disk per disk on the system.
//   - struct disk_ops: This struct contains pointers to functions implementing
//   read and write operation on a particular kind of disk. Therefore there is
//   one struct disk_ops per kind of disk device on the system. The struct disk
//   contains a pointer to a disk_ops.

// Forward definition for pointers in the struct disk_ops below.
struct disk;

// This struct contains the operations that must be supported by a disk. This is
// the lowest layer of the disk driver.
struct disk_ops {
    // Get the sector size used by the disk.
    // @param disk: The disk.
    // @return: The sector size in bytes.
    uint32_t (*sector_size)(struct disk * const disk);

    // Read a sector from the disk.
    // @param disk: The disk.
    // @param sector_index: The index of the sector to read from the disk.
    // @param buf: The buffer to read into. This buffer should be of length >
    // sector size.
    // @return: The number of bytes successfully read. This kernel assumes that
    // a disk can read an entire sector or nothing, therefore this function
    // should return sector_size or 0.
    uint32_t (*read_sector)(struct disk * const disk,
                            sector_t const sector_index,
                            uint8_t * const buf);

    // Write a sector to the disk.
    // @param disk: The disk.
    // @param sector_index: The index of the sector to write to the disk.
    // @param buf: The buffer containing the data to be written. This buffer
    // should be of length > sector size.
    // @return: The number of bytes successfully written. This kernel assumes
    // that a disk can read an entire sector or nothing, therefore this function
    // should return sector_size or 0.
    uint32_t (*write_sector)(struct disk * const disk,
                             sector_t const sector_index,
                             uint8_t const * const buf);
};

// Struct representing a disk on the system. Unlike disk_ops, there is one
// struct disk per disk on the system.
struct disk {
    // The operation offered by this disk.
    struct disk_ops const * ops;

    // Additional data available for the driver.
    void * data;
};

// Read data from a disk.
// @param disk: The disk to read from.
// @param addr: The address on the disk to read from.
// @param buf: The buffer to read the data into.
// @param len: The number of bytes to read.
// @return: The number of bytes successfully read.
size_t disk_read(struct disk * const disk,
                 void const * const addr,
                 uint8_t * const buf,
                 size_t const len);

// Write data to a disk.
// @param disk: The disk to write to.
// @param addr: The address on the disk to read from.
// @param buf: The buffer containing the data to write onto disk.
// @param len: The number of bytes to read.
// @return: The number of bytes successfully read.
size_t disk_write(struct disk * const disk,
                  void const * const addr,
                  uint8_t const * const buf,
                  size_t const len);

// Execute disk related tests.
void disk_test(void);
