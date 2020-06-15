#include <vfs.h>
#include <fs.h>
#include <debug.h>
#include <list.h>
#include <lock.h>
#include <kmalloc.h>
#include <string.h>

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

void init_vfs(void) {
    list_init(&MOUNTS);
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

struct file *vfs_open(pathname_t const filename) {
    struct mount const * const mount = find_mount_for_file(filename);

    if (!mount) {
        PANIC("Cannot find mount point for file %s\n", filename);
    }

    pathname_t const rel_name = filename + strlen(mount->mount_point);
    return mount->fs->ops->open_file(mount->disk, rel_name);
}

void vfs_close(struct file * const file) {
    // FIXME: There is no way to get the mount from an already mounted file,
    // this is because the path field in struct file contains the path relative
    // to the file system, not the absolute path. Hence re-detect the fs on the
    // file's disk to be able to call the right close_file() function.
    struct fs const * const fs = get_fs_for_disk(file->disk);
    ASSERT(fs);

    // For now, the FS are responsible to allocate and de-allocate the struct
    // file* in open_file() and close_file(). Hence, nothing to do after the
    // call to close_file().
    fs->ops->close_file(file);
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
