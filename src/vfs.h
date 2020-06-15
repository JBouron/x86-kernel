#pragma once
#include <disk.h>
#include <types.h>

// This file contains VFS (Virtual File System) related functions. Note that
// this is a _very_ simple implementation of VFS which is in no way near what
// Linux's VFS is capable of.
// The goal of this VFS is to be able to mount disks at certain path and make
// opening, closing, deleting, reading from and writing to files easier.
// The current implementation of VFS has undefined behaviour if the same file is
// opened twice and written and read concurrently (or written twice).

// Initialize VFS.
void init_vfs(void);

// Mount a disk on a particular path.
// @param disk: The disk to be mounted.
// @param target: The path on which the disk should be mounted. This path must
// be an absolute path with a trailing '/' character.
void vfs_mount(struct disk * const disk, pathname_t const target);

// Unmount a disk from a path.
// @param pathname: The path to be unmounted.
void vfs_unmount(pathname_t const pathname);

// Open a file from VFS.
// @param filename: The absolute path of the file to be opened.
// @return: The struct file * associated with the file requested. If no such
// file is found, NO_FILE is returned instead.
struct file *vfs_open(pathname_t const filename);

// Close an opened file.
// @param file: The file to be closed.
void vfs_close(struct file * const file);

// Read from a file.
// @param file: The file to read from.
// @param offset: The offset at which the read should be performed in the file.
// @param buf: The buffer to read into.
// @param len: The length in bytes of the read/buffer.
// @return: The number of bytes successfully read into buf.
size_t vfs_read(struct file * const file,
                off_t const offset,
                uint8_t * const buf,
                size_t const len);

// Write to a file.
// @param file: The file to write into.
// @param offset: The offset at which the write should be performed in the file.
// @param buf: The buffer to write.
// @param len: The length in bytes of the write/buffer.
// @return: The number of bytes successfully written to the file.
size_t vfs_write(struct file * const file,
                 off_t const offset,
                 uint8_t const * const buf,
                 size_t const len);

// Delete a file.
// @param filename: The absolute path of the file to be deleted.
void vfs_delete(pathname_t const filename);

// Execute VFS tests.
void vfs_test(void);
