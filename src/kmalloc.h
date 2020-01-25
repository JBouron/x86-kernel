#pragma once

// Allocate memory.
// @param size: The requested amount of memory.
// @return: The virtual address of the allocated memory.
void * kmalloc(size_t const size);

// Free allocated memory.
// @param addr: The address of the memory to free. This cannot point in the
// middle of an allocated buffer. It _must_ point to the very first byte of the
// allocated buffer.
void kfree(void * const addr);

// Execute memory allocation tests.
void kmalloc_test(void);
