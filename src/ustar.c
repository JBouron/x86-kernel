#include <debug.h>
#include <kmalloc.h>
#include <string.h>
#include <math.h>
#include <memory.h>
#include <fs.h>
#include <disk.h>

// Implementation of the USTAR filesystem. This filesystem is no more than a
// read only TAR archive. It is used for the init ram-disk.

// The format of a TAR file (and hence of this filesystem) is straightforward:
//      HEADER | DATA * | HEADER | DATA * | ....
// Everyfile is preceded by a header (defined by struct ustar_header)
// immediately followed by its data. Everything (header and data) are organized
// in 512 bytes blocks. The length of a file is rounded up to the next multiple
// of 512. Files might not have data block (if their size is 0), in this case
// their header is followed immediately by the header of the next file.
// Directories are equivalent to empty files.

// TAR uses 512 bytes sectors.
#define USTAR_SEC_SIZE  512

// The layout of a file/dir header.
struct ustar_header {
    union {
        // This makes sure that the size of the header is 512.
        char raw[512];
        struct {
            // The name of the file. Note that this may not be the full name if
            // a filename prefix is used (see: filename_prefix field).
            char filename[100];
            // The file's mode. Unused in this kernel.
            uint64_t file_mode;
            // The numeric user ID of the owner of the file. Unused in this
            // kernel.
            uint64_t user_id;
            // The numeric group ID of the group of the file. Unused in this
            // kernel.
            uint64_t group_id;
            // The size of the file encoded as an ASCII octal string.
            char filesize[12];
            // The last modification time in numeric Unix time format (octal).
            char last_mod_time[12];
            // Checksum of the header. See
            // https://en.wikipedia.org/wiki/Tar_(computing)#Header for how to
            // compute it.
            uint64_t checksum; 
            // The type of the file, can be the following:
			//     '0' or (ASCII NUL) 	Normal file
			//     '1' 	Hard link
			//     '2' 	Symbolic link
			//     '3' 	Character device
			//     '4' 	Block device
			//     '5' 	Directory
			//     '6' 	Named pipe (FIFO)
            char type;
			// Name of the linked file. Unused in this kernel as links are not
            // yet supported.
            char linked_file_name[100];
            // This field must be a string spelling "ustar\0".
            char indicator[6];
            // The USTAR version, must be "00".
            char version[2];
            // The name of the file's owner. Unused in this kernel.
            char owner_username[32];
            // The name of the file's group. Unused in this kernel.
            char owner_groupname[32];
            // Unused.
            uint64_t device_major_number;
            // Unused.
            uint64_t device_minor_number;
            // Prefix for the filename. 
            char filename_prefix[155];
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct ustar_header) == 512, "");

// Check if the header describes a directory.
// @param header: The header.
// @return: true if the header describes a directory, false otherwise.
#define is_dir(header)  ((header)->type == '5')

// Check if the header describes a file.
// @param header: The header.
// @return: true if the header describes a file, false otherwise.
#define is_file(header) ((header)->type == '0' || (header)->type == '\0')

// Convert an ASCII octal string to uint64_t.
// @param str: The ASCII octal string. This string must only contain characters
// 0, 1, ..., or 8.
// @return: The base 10 equivalent in a uint64_t.
static uint64_t octal_string_to_u64(char const * const str) {
    uint64_t res = 0;
    for (uint8_t i = 0; i < strlen(str); ++i) {
        char const cdigit = str[i];
        ASSERT('0' <= cdigit && cdigit <= '8');
        res *= 8;
        res += cdigit - '0';
    }
    return res;
}

// Read a USTAR header from initrd.
// @param disk: The disk to read the header from.
// @param offset: The offset of the header.
// @param hdr: The struct ustar_header to read into.
// @return: true if the header was read successfully, false otherwise.
static bool read_header(struct disk * const disk,
                        off_t const offset,
                        struct ustar_header * const hdr) {
    ASSERT(offset % USTAR_SEC_SIZE == 0) ;
    uint8_t * const dest = (uint8_t*)hdr;
    return disk_read(disk, offset, dest, sizeof(*hdr)) == USTAR_SEC_SIZE;
}

// Find a file given its path in a USTAR filesystem.
// @param disk: The disk to look into.
// @param filepath: The path of the file we are looking for.
// @param file_offset[out]: If the file is found, the pointed uint32_t will
// contain the offset of the USTAR header of the file on disk. Otherwise it is
// untouched.
// @return: true if the file was found, false otherwise.
static bool find_file(struct disk * const disk,
                      char const * const filepath,
                      uint32_t *file_offset) {
    struct ustar_header * const hdr = kmalloc(sizeof(*hdr));
    uint32_t offset = 0x0;
    bool found = false;

    while (read_header(disk, offset, hdr)) {
        char * const prefix = hdr->filename_prefix;
        size_t const prefix_len = strlen(prefix);
        char * const filename = hdr->filename;
        size_t const filename_len = strlen(filename);
        size_t const full_filename_len = prefix_len + strlen(filename);

        char * const fullname = kmalloc(full_filename_len + 1);

        if (prefix_len) {
            // Most of the time, there is no prefix.
            memcpy(fullname, prefix, prefix_len);
        }
        memcpy(fullname + prefix_len, filename, filename_len);

        if (streq(fullname, filepath)) {
            // File is found.
            found = true;
            break;
        } else {
            // Skip header.
            offset += sizeof(*hdr);

            // If this was a file, skip any sector containing data as well.
            if (is_file(hdr)) {
                uint64_t const size = octal_string_to_u64(hdr->filesize);
                offset += ceil_x_over_y_u32(size, 512) * 512;
            }
        }
    }
    if (found) {
        *file_offset = offset;
    }
    kfree(hdr);
    return found;
}

// Each opened file on a USTAR file system uses the fs_private field to point to
// the following struct. This is meant as an optimization to avoid scanning the
// entire filesystem every time one wants to read from the same file.
struct ustar_file_private_data {
    // The offset of the header of this file on disk.
    off_t header_offset;
    // A copy of the header of this file on disk.
    struct ustar_header header;
};

// Read a file stored on a USTAR filesystem.
// @param file: The file to read from.
// @param offset: The offset to read from in the file.
// @param buf: The buffer to read into.
// @parma len: The length of the buffer to read.
static size_t ustar_read(struct file * const file,
                         off_t const offset,
                         uint8_t * const buf,
                         size_t const len) {
    struct ustar_file_private_data const * const data = file->fs_private;
    struct ustar_header const * const file_header = &data->header;
    ASSERT(file_header);

    uint64_t const file_len = octal_string_to_u64(file_header->filesize);

    uint32_t const data_off = data->header_offset + sizeof(*file_header);
    off_t const read_off = data_off + offset;
    size_t const read_len = min_u32(file_len - offset, len);

    size_t const read = disk_read(file->disk, read_off, buf, read_len);
    if (read != read_len) {
        WARN("Couldn't read entire buffer from %s\n", file->path);
    }
    return read;
}

// Write a file stored on a USTAR filesystem.
// @param file: The file to write to.
// @param offset: The offset to write to in the file.
// @param buf: The buffer containing the data to be written.
// @parma len: The length of the buffer to write.
// Note: Writting in USTAR filesystems is currently not supported, this is
// because such filesystems are used as initrd images and hence are read-only.
static size_t ustar_write(struct file * file,
                          off_t const offset,
                          uint8_t const * buf,
                          size_t const len) {
    return 0;    
}

// The file operations on a USTAR filesystem.
static struct file_ops ustar_file_ops = {
    .read = ustar_read,
    .write = ustar_write,
};

// Detect if a given disk uses a USTAR filesystem.
// @param disk: The disk to test.
// @return: true if `disk` is using USTAR as its filesystem, false otherwise.
static bool ustar_detect_fs(struct disk * const disk) {
    // Read the first header on disk and check that the indicator spells
    // "ustar".
    struct ustar_header header;
    if (!read_header(disk, 0x0, &header)) {
        // We cannot even read a header, this is clearly not a USTAR filesystem.
        return false;
    } else {
        char const * const expected = "ustar";
        return strneq(header.indicator, expected, strlen(expected));
    }
}

// Create a file on a USTAR filesystem.
// @param disk: The disk to create the new file to.
// @param path: The path of the file to be created.
// @return: A struct file* corresponding to the new file.
// Note: Not supported on USTAR filesystems.
static struct file *ustar_create_file(struct disk * const disk,
                                      char const * const path) {
    return NO_FILE;
}

// Delete a file from a USTAR filesystem.
// @param disk: The disk to delete the file from.
// @param path: The path of the file to be deleted.
static void ustar_delete_file(struct disk * const disk,
                              char const * const path) {
}

// Open a file on a USTAR filesystem.
// @param disk: The disk to open the file from.
// @param path: The full path of the file to be opened.
// @return: A struct file for the opened file.
static struct file *ustar_open_file(struct disk * const disk,
                                    char const * const path) {
    uint32_t file_offset = 0;
    if (!find_file(disk, path, &file_offset)) {
        return NO_FILE;
    }

    struct file * const file = kmalloc(sizeof(*file));
    file->path = path;
    file->disk = disk;
    file->ops = &ustar_file_ops;

    // Read the USTAR header for the file and store it into the fs_private
    // field, this will be used later when reading the file.
    struct ustar_file_private_data * const data = kmalloc(sizeof(*data));
    data->header_offset = file_offset;
    bool const success = read_header(disk, file_offset, &data->header);
    ASSERT(success);
    file->fs_private = data;

    return file;
}

// Close an opened file on a USTAR filesystem.
// @param file: The file to be closed.
static void ustar_close_file(struct file * const file) {
    // Free the ustar_file_private_data.
    kfree(file->fs_private);
}

// The filesystem operations for a USTAR filesystem.
struct fs_ops const ustar_fs_ops = {
    .detect_fs = ustar_detect_fs,
    .create_file = ustar_create_file,
    .open_file = ustar_open_file,
    .close_file = ustar_close_file,
    .delete_file = ustar_delete_file,
};

// The USTAR filesystem implementation.
struct fs const ustar_fs = {
    .name = "USTAR",
    .ops = &ustar_fs_ops,
};

// Pretty print a USTAR header in the logs.
// @param header: The header to pretty print.
__attribute__((unused))
static void pretty_print_header(struct ustar_header const * const header) {
    LOG("USTAR header:\n");
    LOG("  filename            = %s\n", header->filename);
    LOG("  file_mode           = %s\n", &header->file_mode);
    LOG("  user_id             = %s\n", &header->user_id);
    LOG("  group_id            = %s\n", &header->group_id);
    LOG("  filesize            = %s (%u)\n", header->filesize,
        octal_string_to_u64(header->filesize));
    LOG("  last_mod_time       = %s\n", header->last_mod_time);
    LOG("  checksum            = %X\n", header->checksum);
    LOG("  type                = %c\n", header->type);
    LOG("  linked_file_name    = %s\n", header->linked_file_name);
    LOG("  indicator           = %s\n", header->indicator);
    LOG("  version             = %s\n", header->version);
    LOG("  owner_username      = %s\n", header->owner_username);
    LOG("  owner_groupname     = %s\n", header->owner_groupname);
    LOG("  device_major_number = %X\n", header->device_major_number);
    LOG("  device_minor_number = %X\n", header->device_minor_number);
    LOG("  filename_prefix     = %s\n", header->filename_prefix);
}

// Log all the headers found in a USTAR filesystem. This function is meant to be
// used for debugging purposes.
// @param disk: The disk to read from.
__attribute__((unused))
static void log_ustar(struct disk * const disk) {
    struct ustar_header * const hdr = kmalloc(sizeof(*hdr));
    size_t read = 0;
    off_t off = 0x0;
    while ((read = disk_read(disk, off, (uint8_t*)hdr, sizeof(*hdr)))) {
        ASSERT(read == sizeof(*hdr));
        pretty_print_header(hdr);

        off += sizeof(*hdr);
        if (is_file(hdr)) {
            uint64_t const size = octal_string_to_u64(hdr->filesize);
            off += ceil_x_over_y_u32(size, 512) * 512;
        }
    }
}
