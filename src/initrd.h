#pragma once
#include <disk.h>

// This file contains definitions of functions used to interact with the initrd.
// The assumption is that there is a single initrd on the system. This module
// provides an abstraction on the initrd (through a struct disk).

// Init the initial ram-disk driver.
void init_initrd_driver(void);

// Get the struct disk abstraction of the initrd.
// @return: A pointer on the struct disk capable of reading the content of the
// initrd.
struct disk *get_initrd_disk(void);
