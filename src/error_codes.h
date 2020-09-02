#pragma once

// This file contains all the error codes used in the kernel.

typedef enum {
    // Special value meant to indicate that the error code of the current error
    // should be the same as the error code of the previous error causing this
    // one. SET_ERROR() will replace with the appropriate value and ensure that
    // there is indeed a previous error.
    // A use case of this value is when calling SET_ERROR() after a particular
    // function failed but we don't know why.
    // This does NOT indicate success.
    ENONE = -1,

    // Misc error codes
    // ================
    // Out of memory. This can mean both physical or virtual.
    ENOMEM = -2,

    // Filesystem error codes
    // ======================
    // Filesystem not supported by kernel.
    ENOFSIMPL = -1024,
    // Mount point already mounted.
    EMOUNTED = -1025,
    // Path is not a mount point.
    ENOTMOUNTPOINT = -1026,
    // File/resource not found.
    ENOTFOUND = -1027,
} error_code_t;
