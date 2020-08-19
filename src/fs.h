#pragma once
#include <types.h>
#include <list.h>
#include <atomic.h>

// This file declares the filesystem interface. Any supported filesystem must
// define specific functions and structs described below:

// Forward declaration see definition below.
struct file;

// This structure contains pointers to functions operating on files. Those
// functions are defined by the filesystem implementation. There is one struct
// file_ops per filesystem and this struct will be passed to any opened file on
// a filesystem.
struct file_ops {
    // Read from the file.
    // @param file: The file to read from.
    // @param offset: The offset where to read from within the file.
    // @param buf: The buffer to read into.
    // @param len: The length of the buffer in bytes.
    // @return: The number of bytes read.
    size_t (*read)(struct file * file, off_t offset, uint8_t * buf, size_t len);

    // Write to a file.
    // @param file: The file to write into.
    // @param offset: The offset where to write from within the file.
    // @param buf: The buffer containing data to be written.
    // @param len: The length of the buffer in bytes.
    // @return: The number of bytes written.
    size_t (*write)(struct file * file,
                    off_t offset,
                    uint8_t const * buf,
                    size_t len);
};

// Each file opened on the system has a corresponding struct file associated to
// it that contains the state of this file. Note that if a given file on a disk
// is opened by multiple processes (or multiple times by the same process) then
// there is a single shared struct file* between all.
struct file {
    // The absolute path of this file.
    char const * abs_path;
    // The path of this file relative to the fs it is stored on. That is, if the
    // absolute path is /mydir/subdir/a and the fs is mounted at /mydir/, then
    // this field would be subdir/a. Note that there is no '/' at the begining
    // of the path.
    char const * fs_relative_path;
    // The disk from which this file has been opened.
    struct disk * disk;
    // The file operations as defined by the filesystem from which this file was
    // opened.
    struct file_ops const * ops;
    // Available data to the filesytem implementation.
    void *fs_private;

    // Node for the Opened Files Linked List (OFLL) in VFS.
    struct list_node opened_files_ll;
    // Reference count to know how many processes are referencing this file.
    // When this reaches 0, this struct can be removed from the OFLL and freed.
    atomic_t open_ref_count;
};

// Special value to be used when a file cannot be found or created.
#define NO_FILE NULL

// Each supported filesystem must define a struct fs which defines basic
// operations on the filesystem. Note: If a given filesystem is used twice (i.e.
// on two different disks), only one such struct is defined and used.
struct fs_ops {
    // Try to detect the file system on a disk. This function is used when the
    // file system on a disk is unknown.
    // @param disk: The disk.
    // @return: true if the disk uses this filesystem, false otherwise. 
    bool (*detect_fs)(struct disk * disk);

    // Create a new file on the filesystem.
    // @param disk: The disk on which the new file should be created.
    // @param path: The full path of the file to be created.
    // @return: A struct file for the new created file. If the file cannot be
    // created for any reason, NO_FILE should be returned.
    struct file *(*create_file)(struct disk * disk, char const * path);

    // Open a file on the filesystem.
    // @param disk: The disk to open the file from.
    // @param path: The full path of the file to be opened.
    // @return: A struct file for the opened file. If no such file exists on the
    // filesystem, this function should return NO_FILE.
    struct file *(*open_file)(struct disk * disk, char const * path);

    // Close an opened file on the filesystem.
    // @param file: The file to be closed.
    void (*close_file)(struct file * file);

    // Delete a file from the filesystem.
    // @param disk: The disk to delete the file from.
    // @param path: The full path of the file to be deleted.
    void (*delete_file)(struct disk * disk, char const * path);
};

// This struct describes a supported filesystem in this kernel. There is one
// such struct per supported filesystem no matter how many mounted disks are
// using this filesystem.
struct fs {
    // The name of the supported file system.
    char const * name;
    // The operations available on the filesystem.
    struct fs_ops const * ops;
};
