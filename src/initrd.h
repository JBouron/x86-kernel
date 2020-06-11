#pragma once
#include <disk.h>

// This file contains definitions of functions used to interact with the initrd.
// The assumption is that there at most initrd on the system. This module
// provides an abstraction on the initrd (through a memdisk).

// Get a memdisk of the initrd.
// @return: A pointer on the struct disk capable of reading the content of the
// initrd. If no initrd has been loaded by the bootloader then this function
// returns NULL.
struct disk *get_initrd_disk(void);
