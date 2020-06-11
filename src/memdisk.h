#pragma once
#include <disk.h>
#include <types.h>

// This file contains functions to create and delete memdisks.
// memdisks are virtual disks that reside in RAM. Just like physical disk one
// can read and write sectors to memdisks. Memdisk must have a size that is a
// multiple of 512 bytes.

// Create a memdisk.
// @param addr: The address pointing to the start of the memory region to be
// used as a memdisk.
// @param size: The size of the memdisk in bytes. Must be a multiple of 512
// bytes.
// @param read_only: If true, the resulting memdisk will be read only.
// @return: The struct disk* associated to the new memdisk.
struct disk *create_memdisk(void * const addr,
                            size_t const size,
                            bool const read_only);

// Delete a memdisk.
// @param disk: The memdisk to be deleted.
// Note: This function will de-allocate the struct disk*.
void delete_memdisk(struct disk * const disk);

// Run memdisk tests.
void memdisk_test(void);
