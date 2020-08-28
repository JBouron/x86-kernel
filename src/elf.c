#include <elf.h>
#include <debug.h>
#include <vfs.h>
#include <kmalloc.h>
#include <memory.h>
#include <paging.h>
#include <math.h>
#include <frame_alloc.h>

// Typedef all the types per the ELF specification.
typedef void *elf32_addr_t;
typedef uint16_t elf32_half_t;
typedef uint32_t elf32_off_t;
typedef int32_t elf32_sword_t;
typedef uint32_t elf32_word_t;

// The type of the ELF header.
enum elf32_ehdr_type {
    // No file type.
    ET_NONE = 0,
    // Relocatable file.
    ET_REL = 1,
    // Executable file.
    ET_EXEC = 2,
    // Shared object file.
    ET_DYN = 3,
    // Core file.
    ET_CORE = 4,
    // Processor specific.
    ET_LOPROC = 0xff00,
    // Processor specific.
    ET_HIPROC = 0xffff,
} __attribute__((packed));
STATIC_ASSERT(sizeof(enum elf32_ehdr_type) == sizeof(elf32_half_t), "");

// The architecture targeted by an ELF.
enum elf32_ehdr_machine {
    EM_NONE = 0,
    EM_M32 = 1,
    EM_SPARC = 2,
    EM_386 = 3,
    EM_68K = 4,
    EM_88K = 5,
    EM_860 = 7,
    EM_MIPS = 8,
    __EM__SIZE = 0xFFFF,
} __attribute__((packed));
STATIC_ASSERT(sizeof(enum elf32_ehdr_machine) == sizeof(elf32_half_t), "");

// The `class` of the ELF, that is whether is is 32 bits or 64 bits.
enum elf32_ehdr_class {
    // Invalid class.
    ELF_CLASS_NONE = 0,
    // 32-bit ELF file.
    ELF_CLASS_32 = 1,
    // 64-bit ELF file.
    ELF_CLASS_64 = 2,
} __attribute__((packed));
STATIC_ASSERT(sizeof(enum elf32_ehdr_class) == sizeof(unsigned char), "");

// The endianness of the ELF.
enum elf32_ehdr_endianness {
    // Invalid endianness.
    ELF_ENDIANNESS_NONE = 0,
    ELF_ENDIANNESS_LSB = 1,
    ELF_ENDIANNESS_MSB = 2,
} __attribute__((packed));
STATIC_ASSERT(sizeof(enum elf32_ehdr_endianness) == sizeof(unsigned char), "");

// ELF header identification data. This represents the first 16 bytes of the ELF
// header.
struct elf32_ehdr_ident {
    union {
        uint8_t padding[16];
        struct {
            // The magic string should contain: '\x7f','E','L','F'.
            char magic[4];
            // The class (32 or 64 bits).
            enum elf32_ehdr_class class;
            // The endianness of the executable in the file.
            enum elf32_ehdr_endianness endianness;
            // The version of ELF. Must be 1.
            unsigned char version;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));
STATIC_ASSERT(sizeof(struct elf32_ehdr_ident) == 16, "");

// Layout of the ELF header.
struct elf32_ehdr {
    // File identifier.
    struct elf32_ehdr_ident ident;
    // Object file type, see corresponding enum for values.
    enum elf32_ehdr_type type;
    // Target machine architecture.
    enum elf32_ehdr_machine machine;
    // Object file version. Must be 1.
    elf32_word_t version;
    // Virtual address at which the system should transfer control to the
    // process, that is the entry point of the process.
    elf32_addr_t entry;
    // Offset of the program header table in the file. 0 if not present.
    elf32_off_t phoff;
    // Offset of the section header table in the file. 0 if not present.
    elf32_off_t shoff;
    // Processor specific flags.
    elf32_word_t flags;
    // ELF header's size in bytes.
    elf32_half_t ehsize;
    // Size of entries in the program header table in bytes. All entries have
    // the same size.
    elf32_half_t phentsize;
    // Number of entries in the program header table. If no entries then this is
    // 0. The product of the entry size and the number of entries should give
    // the size of the program header table above.
    elf32_half_t phnum;
    // Size of entries in the section header table in bytes. All entries have
    // the same size.
    elf32_half_t shentsize;
    // Number of entries in the section header table. If no entries then this is
    // 0. The product of the entry size and the number of entries should give
    // the size of the section header table above.
    elf32_half_t shnum;
    // Section header table index of the entry associated with the section name
    // string table. If no such section, this field should be SHN_UNDEF.
    elf32_half_t shstrndx;
} __attribute__((packed));

// The type of a program header table entry.
enum elf32_phdr_type {
    // This entry is unused and should be ignored. Other fields have undefined
    // values.
    PT_NULL = 0,
    // This is a loadable segment. If memsz > filesz the extra bytes must be
    // zeroed and must follow the segment's initialized data.
    PT_LOAD = 1,
    // Dynamic linking information.
    PT_DYNAMIC = 2,
    // The element specifies the location and size of a null-terminated path
    // name to invoke as an interpreter.
    PT_INTERP = 3,
    // The element specifies the location and size of auxiliary information.
    PT_NOTE = 4,
    // Reserved, should never be used.
    PT_SHLIB = 5,
    // This segment specifies the location and size of the program header table
    // itself both in the file and in the memory image of the program.
    PT_PHDR = 6,
    // LO and HI ranges are reserved for processor-specific semantics.
    PT_LOPROC = 0x70000000,
    PT_HIPROC = 0x7FFFFFFF,
} __attribute__((packed));
STATIC_ASSERT(sizeof(enum elf32_phdr_type) == sizeof(elf32_word_t), "");

// Layout of a program header in an ELF. Each such struct represent a segment of
// the process image.
struct elf32_phdr {
    // Type of segment.
    enum elf32_phdr_type type;
    // Offset from the beginning of the file at which the first byte of the
    // segment resides.
    elf32_off_t offset;
    // The virtual address at which the first byte of the segment should reside
    // in memory.
    elf32_addr_t vaddr;
    // Unused. (This used to hold the physical address of the segment).
    elf32_addr_t paddr;
    // Number of bytes in the file of the segment; may be 0.
    elf32_word_t filesz;
    // Number of bytes in memory of the segment; may be 0.
    elf32_word_t memsz;
    // Flags relevent to the segment. See macros below for values.
    elf32_word_t flags;
    // Alignment of the segment.
    elf32_word_t align;
} __attribute__((packed));
// This segment has executable permissions.
#define PHDR_FLAG_EXEC  0x1
// This segment is writable.
#define PHDR_FLAG_WRITE 0x2
// This segment is readable.
#define PHDR_FLAG_READ  0x4

// Check that the elf header is describing an ELF supported by this kernel. This
// means:
//     - Magic and version values are as expected.
//     - The ELF is an executable file.
//     - The target machine is Intel 80386.
//     - The class is 32 bits.
//     - This is little endian.
// @param header: The header to check.
// @return: true if the header is valid _and_ supported by this kernel, false
// otherwise.
static bool check_elf_header(struct elf32_ehdr const * const header) {
    return header->ident.magic[0] == 0x7F &&
        header->ident.magic[1] == 'E' &&
        header->ident.magic[2] == 'L' &&
        header->ident.magic[3] == 'F' &&
        header->ident.class == ELF_CLASS_32 &&
        header->ident.endianness == ELF_ENDIANNESS_LSB &&
        header->ident.version == 1 &&
        header->machine == EM_386 &&
        header->version == 1;
}

// Convert the flags value coming from a program header to the flags used by the
// paging functions.
// @param flags: The value of the flags, as it appear in the program header.
// @return: The flags to be used when mapping the segment.
static uint32_t segment_flags_to_paging_flags(elf32_word_t const flags) {
    uint32_t paging_flags = VM_USER | VM_NON_GLOBAL;
    if (flags & PHDR_FLAG_WRITE) {
        paging_flags |= VM_WRITE;
    }
    return paging_flags;
}

// Read the ELF header from a file.
// @param file: The file to read from.
// @param dst: The destination to read the header into.
static void read_elf_header(struct file * const file, struct elf32_ehdr * dst) {
    size_t const read = vfs_read(file, 0x0, (uint8_t*)dst, sizeof(*dst));
    ASSERT(read == sizeof(*dst));
    ASSERT(check_elf_header(dst));
}

// Compute the number of frames/pages needed to hold a memory range.
// @param start: Pointer on the first byte of the memory range.
// @param end: Pointer on the last byte of the memory range.
// @return: The number of frames/pages required to hold that range.
static size_t get_required_num_frames(void const * const start,
                                      void const * const end) {
    ASSERT(start < end);
    void const * const start_page = get_page_addr(start);
    void const * const end_page = get_page_addr(end);
    size_t const diff = (uint32_t)(end_page - start_page);
    ASSERT(!(diff % PAGE_SIZE));
    return (diff / PAGE_SIZE) + 1;
}

// Process a program header from an ELF. This function will copy the content of
// the segment described by the program header into the proc's address space.
// @param file: The ELF file.
// @param proc: The process to load the ELF into.
// @param elf_hdr: The ELF header of the file.
// @param prog_hdr: The program header to be processed.
static void process_program_header(struct file * const file,
                                   struct proc * const proc,
                                   struct elf32_ehdr const * const elf_hdr,
                                   struct elf32_phdr const * const prog_hdr) {
    // This kernel only supports executable / non-dynamically linked ELFs. This
    // means that we expects on PT_LOAD program header.
    ASSERT(prog_hdr->type == PT_LOAD);

    // This should always be the case since ELFs are targeting i386.
    ASSERT(prog_hdr->align == PAGE_SIZE);

    // This function is supposed to be called while being in the address space
    // of the process.
    ASSERT(get_curr_addr_space() == proc->addr_space);

    // Start and end addresses of the segment in virtual memory.
    void const * const segment_start = prog_hdr->vaddr;
    void const * const segment_end = segment_start + prog_hdr->memsz - 1;

    // Allocate physical frames for this segment.
    size_t const nframes = get_required_num_frames(segment_start, segment_end);
    void **frames = kmalloc(nframes * sizeof(*frames));
    TODO_PROPAGATE_ERROR(!frames);
    for (uint32_t i = 0; i < nframes; ++i) {
        frames[i] = alloc_frame();
        TODO_PROPAGATE_ERROR(frames[i] == NO_FRAME);
    }

    // ELF and paging do not share the same flags for access permissions. We
    // need to translate them.
    uint32_t const map_flags = segment_flags_to_paging_flags(prog_hdr->flags);

    // Map the frames to where the segment should reside in memory.
    void * const first_page = get_page_addr(segment_start);
    void * const mapped = paging_map_frames_above(first_page,
                                                  frames,
                                                  nframes,
                                                  map_flags);
    TODO_PROPAGATE_ERROR(mapped == NO_REGION);
    // FIXME: We could map the frames one by one to make sure that they are
    // mapped at the right virt address. Using paging_map_frames_above_in() is
    // shorter be we could end up in a situation where the mapped address is not
    // the one requested, hence the ASSERT() below.
    ASSERT(mapped == first_page);

    // Copy the data to the process' virtual memory. FIXME: Since there is no
    // easy way to change the access permissions of a mapped virtual memory
    // range, we need to re-map the frames with VM_WRITE permissions in order to
    // do the copy.
    void * const write_map = paging_map_frames_above(0x0,
                                                     frames,
                                                     nframes,
                                                     VM_NON_GLOBAL | VM_WRITE);
    TODO_PROPAGATE_ERROR(write_map == NO_REGION);

    // Copy the initialized segment data from the file.
    off_t const offset = prog_hdr->offset;
    size_t const len = prog_hdr->filesz;
    void * const dest = write_map + (segment_start - first_page);
    size_t const written = vfs_read(file, offset, dest, len);
    ASSERT(written == prog_hdr->filesz);

    // In some cases the size of the segment as it appear in the file is
    // different than what its size should be in memory. This is the case for
    // uninitialized memory for instance. In this case, the ELF specification
    // requires the memory after the initialized segment data to be zeroed.
    ASSERT(prog_hdr->memsz >= prog_hdr->filesz);
    if (prog_hdr->filesz != prog_hdr->memsz) {
        memzero(write_map + written, prog_hdr->memsz - len);
    }

    // Clean up the mapping used for copying.
    paging_unmap(write_map, nframes * PAGE_SIZE);
    kfree(frames);
}

// Read a program header from a ELF file.
// @param file: The ELF file to read from.
// @param elf_hdr: The ELF header of the file.
// @param index: The index of the program header to be read.
// @param dst: The destination to read the program header into.
static void read_program_header(struct file * const file,
                                struct elf32_ehdr const * const elf_hdr,
                                uint32_t const index,
                                struct elf32_phdr * const dst) {
    ASSERT(index < elf_hdr->phnum);
    off_t const offset = elf_hdr->phoff + index * elf_hdr->phentsize;
    size_t const r = vfs_read(file, offset, (uint8_t*)dst, sizeof(*dst));
    ASSERT(r == sizeof(*dst));
}

void load_elf_binary(struct file * const file, struct proc * const proc) {
    ASSERT(file);

    // Read ELF header from the file.
    struct elf32_ehdr header;
    read_elf_header(file, &header);

    // To make copying the segments easier, temporarily switch to the process'
    // address space.
    switch_to_addr_space(proc->addr_space);

    // Process each program header.
    for (elf32_half_t i = 0; i < header.phnum; ++i) {
        struct elf32_phdr phdr;

        read_program_header(file, &header, i, &phdr);
        process_program_header(file, proc, &header, &phdr);
    }

    switch_to_addr_space(get_kernel_addr_space());

    // Set the EIP to the entry point of the program.
    proc->registers_save.eip = (reg_t)header.entry;

    proc->state_flags &= ~PROC_WAITING_EIP;
}

#include <elf.test>
