#pragma once

#ifdef KMALLOC_DEBUG
// When KMALLOC_DEBUG is enabled, each allocation records the file and line
// number where it occured, this information is appended at the end of the
// memory allocated. In this mode the allocated memory has the following layout:
//
//  +-----------+- . . . -+-----------+-----------+-------------+------------+
//  | Orig Size |  Data   | Func name | File name | Line number |    CPU     |
//  +-----------+- . . . -+-----------+-----------+-------------+------------+
//                         <--          struct kmalloc_debug_info         -->
//
// so that:
//  - Orig size (uint32_t) is the original size requested by the kmalloc() call,
//  without the debug info.
//  - Func name (char*) is the function in which the kmalloc() call originated
//  from.
//  - File name (char*) is the file in which the kmalloc() call originated from.
//  This is a _pointer_ not a full string.
//  - Line number (uint32_t) is the line in the file in which the call occured.
//  - CPU (uint8_t) is the CPU that requested the memory.
//
// Note: The kmalloc() call will return the address of the first byte of Data,
// kfree() expects this same address. This allows transparency, the caller will
// have no idea of the debug info being there.

// Wrapper of the original kmalloc() function implementing the layout described
// above.
// @param func_name: The function in which the kmalloc() macro call originated.
// @param filename: The file from which the kmalloc() macro call originated.
// @param line_number: The line number in the file where the kmalloc() call
// originated.
// @param size: The requested amount of memory.
// @return: The virtual address of the allocated memory.
void * kmalloc_debug_wrapper(char const * const func_name,
                             char const * const filename,
                             uint32_t const line_number,
                             size_t const size);

#define kmalloc(size) \
    kmalloc_debug_wrapper(__func__, __FILE__, __LINE__, size)

// Wrapper of the original kfree() function implementing the layout described
// above.
// @param func_name: The function in which the kmalloc() macro call originated.
// @param filename: The file from which the kfree() macro call originated.
// @param line_number: The line number in the file where the kfree() call
// originated.
// @param addr: The address of the memory to free. This cannot point in the
// middle of an allocated buffer. It _must_ point to the very first byte of the
// allocated buffer.
void kfree_debug_wrapper(char const * const func_name,
                         char const * const filename,
                         uint32_t const line_number,
                         void * const addr);

#define kfree(addr) \
    kfree_debug_wrapper(__func__, __FILE__, __LINE__, addr)

#else // KMALLOC_DEBUG
// Allocate memory.
// @param size: The requested amount of memory.
// @return: The virtual address of the allocated memory.
void * kmalloc(size_t const size);

// Free allocated memory.
// @param addr: The address of the memory to free. This cannot point in the
// middle of an allocated buffer. It _must_ point to the very first byte of the
// allocated buffer.
void kfree(void * const addr);
#endif

// Compute the number of total bytes currently allocated through kmalloc.
size_t kmalloc_total_allocated(void);

// Execute memory allocation tests.
void kmalloc_test(void);
