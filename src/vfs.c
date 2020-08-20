#include <vfs.h>
#include <fs.h>
#include <debug.h>
#include <list.h>
#include <lock.h>
#include <kmalloc.h>
#include <string.h>
#include <memory.h>

extern struct fs const ustar_fs;

// This array contains a pointer to the struct fs of each supported filesystem.
// Upon mounting a new disk, VFS will lookup filesystem from this array to
// detect which one is used on the disk.
static struct fs const * const SUPPORTED_FS[] = {
    &ustar_fs,
};

// Describe a mount in VFS.
struct mount {
    // The path on which a disk is mounted.
    pathname_t mount_point;
    // The disk mounted.
    struct disk * disk;
    // The filesystem used by the disk.
    struct fs const * fs;
    // Linked list of struct mounts.
    struct list_node mount_point_list;
};

// A linked list containing all mounts in VFS.
static struct list_node MOUNTS;
// A lock to protect the MOUNTS linked-list.
static DECLARE_SPINLOCK(MOUNTS_LOCK);

// A linked list of all the opened struct files * on the system. This linked
// list is used to share struct file * between processes wishing to open the
// same file. AKA Opened Files Linked List (OFLL).
// TODO: Later on, this should be implemented as a hash map filename => file*.
static struct list_node OPENED_FILES;
// Lock for the OPENED_FILES list.
static DECLARE_SPINLOCK(OPENED_FILES_LOCK);

void init_vfs(void) {
    list_init(&MOUNTS);
    list_init(&OPENED_FILES);
}

// Detect the filesystem used on a disk.
// @param disk: The disk to detect the filesystem from.
// @return: A pointer on the struct fs of the filesystem used by the disk. If no
// filesystem was detected (or filesystem not supported), return NULL.
static struct fs const *get_fs_for_disk(struct disk * const disk) {
    size_t const num_fs = sizeof(SUPPORTED_FS) / sizeof(*SUPPORTED_FS);
    for (size_t i = 0; i < num_fs; ++i) {
        struct fs const * const fs = SUPPORTED_FS[i];
        if (fs->ops->detect_fs(disk)) {
            return fs;
        }
    }
    return NULL;
}

// Check that a target for a mount is valid, that is this is an absolute path
// and there is a trailing '/'.
// @param pathname: The pathname to check validity.
// @return: true if the pathname is valid (per description above), false
// otherwise.
static bool mount_target_is_valid(pathname_t const pathname) {
    size_t const len = strlen(pathname);
    return len && (pathname[0] == '/') && (pathname[len - 1] == '/');
}

void vfs_mount(struct disk * const disk, pathname_t const target) {
    ASSERT(mount_target_is_valid(target));
    struct mount * const mount = kmalloc(sizeof(*mount));
    mount->mount_point = target;
    mount->disk = disk;
    mount->fs = get_fs_for_disk(disk);
    if (!mount->fs) {
        PANIC("Unsupported filesystem for disk %p.\n", disk);
    }
    list_init(&mount->mount_point_list);

    spinlock_lock(&MOUNTS_LOCK);

    // Check that the desired target is not already used by another mount.
    struct mount const * it = NULL;
    list_for_each_entry(it, &MOUNTS, mount_point_list) {
        if (streq(it->mount_point, target)) {
            PANIC("%s is already mounted.\n", target);
        }
    }

    list_add_tail(&MOUNTS, &mount->mount_point_list);
    spinlock_unlock(&MOUNTS_LOCK);
}

void vfs_unmount(pathname_t const pathname) {
    spinlock_lock(&MOUNTS_LOCK);

    struct mount * mount = NULL;
    list_for_each_entry(mount, &MOUNTS, mount_point_list) {
        if (streq(mount->mount_point, pathname)) {
            break;
        }
    }

    if (!mount) {
        // This path was never mounted.
        PANIC("Tried to unmount a path that is not a mount point.\n");
    }

    list_del(&mount->mount_point_list);
    spinlock_unlock(&MOUNTS_LOCK);

    kfree(mount);
}

// Check if a pathname is under a given mount.
// @param mount: The mount.
// @param filename: The pathname to test.
// @return: If `filename` is under `mount` then the length of the common prefix
// is returned. Otherwise 0.
static size_t is_under_mount(struct mount const * const mount, 
                             pathname_t const filename) {
    size_t const mount_len = strlen(mount->mount_point);
    size_t const filename_len = strlen(filename);

    if (filename_len < mount_len) {
        // Filename is too short to be under mount.
        return 0;
    }

    for (size_t i = 0; i < mount_len; ++i) {
        if (mount->mount_point[i] != filename[i]) {
            return 0;
        }
    }
    return mount_len;
}

// Find which mount in the MOUNTS linked-list contains a given pathname.
// @param filename: The pathname for which the function should find the
// associated mount.
// @return: The mount containing the pathname, NULL if no such mount exists.
static struct mount const *find_mount_for_file(pathname_t const filename) {
    spinlock_lock(&MOUNTS_LOCK);

    struct mount const * best_mount = NULL;
    struct mount const * mount = NULL;
    size_t longest_common_prefix = 0;

    list_for_each_entry(mount, &MOUNTS, mount_point_list) {
        size_t const common = is_under_mount(mount, filename);
        if (common > longest_common_prefix) {
            longest_common_prefix = common;
            best_mount = mount;
        }
    }

    spinlock_unlock(&MOUNTS_LOCK);
    return best_mount;
}

// Opening/Closing files and the Opened Files Linked List (OFLL)
// =============================================================
//    When opening a file through vfs_open(), we first need to check if this
// file is already opened by looking it up in the OFLL. The reason is that we
// need to maintain the invariant that there is only ONE struct file* per
// pathname, even if this file is opened in multiple processes concurrently.
// When closing a file, care must be taken not to remove it from the OFLL if
// another live process is still using it, this is implemented using a ref count
// in the struct file (see open_ref_count field).
//
// There are two tricky scenarios:
//   - A file is opened for the first time: We need to allocate its struct file*
//   and insert it in the OFLL.
//   - A file is being closed and no other process is using it: We need to
//   remove it from the OFLL and free its associated struct file*.
// Note that in both scenarios, we need to hold the OFLL's lock while
// opening/closing the file. This is because another process might try to open
// the same file concurrently and, if the opening operation is not in the OFLL's
// lock critical section, we might end up with two struct file*.

// Open a file.
// @param filename: The absolute path of the file to be opened.
// @return: The associated struct file*.
static struct file *open_file(pathname_t const filename) {
    // Per the explaination above.
    ASSERT(spinlock_is_held(&OPENED_FILES_LOCK));

    struct mount const * const mount = find_mount_for_file(filename);
    if (!mount) {
        LOG("Cannot find mount point for file %s\n", filename);
        return NULL;
    }
    struct disk * const disk = mount->disk;

    // The filename passed as argument is short lived. Make a copy.
    pathname_t const filename_cpy = memdup(filename, strlen(filename) + 1);
    // Use the same string to represent the absolute and relative paths.
    pathname_t const rel_path = filename_cpy + strlen(mount->mount_point);

    // Allocate the file and initialize all the fields except for the FS
    // specific ones.
    struct file * const file = kmalloc(sizeof(*file));
    file->abs_path = filename_cpy;
    file->fs_relative_path = rel_path;
    file->disk = disk;
    list_init(&file->opened_files_ll);
    atomic_init(&file->open_ref_count, 1);

    // Initialize FS specific fields.
    enum fs_op_res const res = mount->fs->ops->open_file(disk, file, rel_path);
    if (res == FS_SUCCESS) {
        return file;
    } else {
        // abs_path and fs_relative_path are using the same string. Only one
        // free necessary for both.
        kfree((char*)file->abs_path);
        kfree(file);
        return NULL;
    }
}

// Lookup a file into the OFLL.
// @param filename: The absolute path of the file to lookup.
// @return: If the file is present in the list, this function returns the struct
// file* associated with it. Else return NULL.
static struct file *lookup_file(pathname_t const filename) {
    // Per the explaination above.
    ASSERT(spinlock_is_held(&OPENED_FILES_LOCK));

    bool found = false;
    struct file * it;
    list_for_each_entry(it, &OPENED_FILES, opened_files_ll) {
        if (streq(it->abs_path, filename)) {
            // This file has already been opened.
            found = true;
            break;
        }
    }
    return found ? it : NULL;
}

// Look up a file in the OFLL or open the file and insert it into the list.
// @param filename: The absolute path of the file to look up/open.
// @return: The struct file* associated with `filename`.
static struct file *lookup_file_or_open(pathname_t const filename) {
    spinlock_lock(&OPENED_FILES_LOCK);

    struct file *file = lookup_file(filename);
    if (file) {
        // File was already opened, we can return now.
        atomic_inc(&file->open_ref_count);
        spinlock_unlock(&OPENED_FILES_LOCK);
        return file;
    }

    // Open file and insert it into the OFLL.
    file = open_file(filename);
    if (!file) {
        // Could not open the file.
        spinlock_unlock(&OPENED_FILES_LOCK);
        return NULL;
    }

    list_add(&OPENED_FILES, &file->opened_files_ll);

    spinlock_unlock(&OPENED_FILES_LOCK);
    return file;
}

struct file *vfs_open(pathname_t const filename) {
    return lookup_file_or_open(filename);
}

// Close a file. This function must be called on a file that is NOT in the OFLL.
// @param file: The file to be closed.
static void close_file(struct file * const file) {
    // Per the explaination above.
    ASSERT(spinlock_is_held(&OPENED_FILES_LOCK));

    // The file should be removed from the OFLL before being freed.
    ASSERT(!lookup_file(file->abs_path));

    struct mount const * const mount = find_mount_for_file(file->abs_path);
    ASSERT(mount);

    mount->fs->ops->close_file(file);

    // abs_path and fs_relative_path are using the same string. Only one free
    // necessary for both.
    kfree((char*)file->abs_path);

    kfree(file);
}

void vfs_close(struct file * const file) {
    spinlock_lock(&OPENED_FILES_LOCK);
    if (atomic_dec_and_test(&file->open_ref_count)) {
        // This was the last instance of this file. We can now actually close
        // the file and remove it from the OFLL.
        list_del(&file->opened_files_ll);
        close_file(file);
    }
    spinlock_unlock(&OPENED_FILES_LOCK);
}

size_t vfs_read(struct file * const file,
                off_t const offset,
                uint8_t * const buf,
                size_t const len) {
    return file->ops->read(file, offset, buf, len);
}

size_t vfs_write(struct file * const file,
                 off_t const offset,
                 uint8_t const * const buf,
                 size_t const len) {
    return file->ops->write(file, offset, buf, len);
}

void vfs_delete(pathname_t const filename) {
    struct mount const * const mount = find_mount_for_file(filename);

    if (!mount) {
        PANIC("Cannot find mount point for file %s\n", filename);
    }

    pathname_t const rel_name = filename + strlen(mount->mount_point);
    return mount->fs->ops->delete_file(mount->disk, rel_name);
}

#include <vfs.test>
