#include <memory/gdt.h>
#include <memory/segment.h>
#include <asm/asm.h>
#include <memory/memory.h>
#include <includes/debug.h>

// Segment descriptors are 8-bytes value containing, among other things, the
// base address and the limit for a segment.
// This struct encodes this 8-byte value in a more readable way.
// This is the real segment entry representation, and this is what the processor
// expects in the GDT as opposed to gdt_entry which is made to be simpler to
// manipulate.
struct segment_desc_t {
    // Lower 16-bits of the limit.
    uint16_t limit_bits15_to_0 : 16;
    // Some bits of the base.
    uint16_t base_bits15_to_0 : 16;
    uint8_t base_bits23_to_16 : 8;
    // Indicate the type of segment encoded in 4 bits as follows:
    // MSB: if 1, this is a code segment, else a data segment.
    // If this is a code segment the last 3 bits are: C, R and A, where C
    // indicate if the segment is conforming, R if it is readable and A
    // accessed.
    // If this is a data segment, the last 3 bits are: E, W and A where E
    // indicates "Expand-down", W writable and A accessed.
    uint8_t seg_type : 4;
    // 0 = system segment, 1 = code or data segment.
    uint8_t desc_type : 1;
    // The priviledge level/ring required to use the segment.
    uint8_t priv : 2;
    // Is the segment present ?
    uint8_t present : 1;
    // Some bits of the limit.
    uint8_t limit_bits19_to_16 : 4;
    // Available bit. Unused for now.
    uint8_t avl : 1;
    // Should always be 0. (Reserved in non-IAe mode).
    uint8_t is64bits : 1;
    // Operation size: 0 = 16-bit, 1 = 32 bit segment. Should always be 1.
    uint8_t db : 1;
    // Scaling of the limit field. If 1 limit is interpreted in 4KB units,
    // otherwise in bytes units.
    uint8_t granularity : 1;
    // Some bits of the base.
    uint8_t base_bits31_to_24 : 8;
} __attribute__((packed));

// We use specific segments for the kernel.
uint16_t const KERNEL_CODE_SEGMENT_IDX = 1;
uint16_t const KERNEL_DATA_SEGMENT_IDX = 2;
uint16_t const KERNEL_CODE_SEGMENT_SELECTOR = KERNEL_CODE_SEGMENT_IDX<<3|RING_0;
uint16_t const KERNEL_DATA_SEGMENT_SELECTOR = KERNEL_DATA_SEGMENT_IDX<<3|RING_0;

// Most of the fields in segment_desc_t will have fixed value no matter what
// type of segment it describes.
// Thus instead of using the complex segment_desc_t struct, we prefer using the
// following struct to create GDT entries.
struct gdt_entry {
    // The base address of the segment.
    uint32_t base_addr;
    // The limit of the segment, we use a page (4KB) granularity for GDT
    // entries.
    uint32_t limit : 20;
    // The required privilege for the segment.
    uint8_t privilege : 2;
    // The type of the segment it is analogous as the type field in
    // segment_desc_t.
    uint8_t type : 4;
} __attribute__((packed));

// The static list of GDT entries we wish to add to the GDT.
static struct gdt_entry _gdt_entries[] = {
    {0,0,0,0}, // The null segment. It must be present as the first entry.
    {0x0,0xFFFFF,RING_0,0xA}, // Kernel code segment. 0xA = Code/Executable.
    {0x0,0xFFFFF,RING_0,0x2}, // Kernel data segment. 0x2 = Data/Writable.
};

// Declare the *real* GDT array, that is the GDT that will be used by the
// processor. Unlike _gdt_entries, this array must be an array of
// segment_desc_t.
#define GDT_SIZE ((sizeof(_gdt_entries)/sizeof(struct gdt_entry)))
static struct segment_desc_t GDT[GDT_SIZE];

// Convert a struct gdt_entry into a struct segment_desc_t.
static void
convert_gdt_seg_desc_t(struct gdt_entry const* const from,
                       struct segment_desc_t * const to) {
    // Encode the base addr.
    to->base_bits31_to_24 = (0xFF000000&from->base_addr)>>24;
    to->base_bits23_to_16 = (0x00FF0000&from->base_addr)>>16;
    to->base_bits15_to_0  = (0x0000FFFF&from->base_addr)>>0;

    // Encode the limit.
    to->limit_bits19_to_16 = (0xF0000&from->limit)>>16;
    to->limit_bits15_to_0 = (0x0FFFF&from->limit)>>0;

    // Those values will never change:
    to->granularity = 1;     // 4KB units for the limit.
    to->db = 1;              // 32-bit segment.
    to->is64bits = 0;        // We are not in 64-bit mode.
    to->avl = 0;             // Not used, set to 0.
    to->present = 1;         // The segment will obviously be present.

    // Setup the privilege of the segment.
    to->priv = from->privilege;

    // This segment will either be a code or data segment.
    to->desc_type = 1;
    // Setup the access rights.
    to->seg_type = from->type;
}

// Pretty print the addition of a gdt_entry at index `i` in the logs.
static void
_pretty_print_gdt_entry(struct gdt_entry const * const entry, size_t const i) {
    LOG("Add GDT entry [%d] = {\n",i);
    LOG("   .base_addr = %x,\n",entry->base_addr);
    LOG("   .limit     = %x,\n",entry->limit);
    LOG("   .privilege = %x,\n",entry->privilege);
    LOG("   .type      = %x,\n",entry->type);
    LOG("}\n");
}

// Add a gdt_entry to the GDT at index `idx`.
static void
add_gdt_entry(struct gdt_entry const * const entry, size_t const idx) {
    // Since the GDT hold segment_desc_t, we need to convert our entry first.
    struct segment_desc_t seg;
    _pretty_print_gdt_entry(entry,idx);
    if(idx) {
        // Convert the entry to a real segment descriptor.
        convert_gdt_seg_desc_t(entry,&seg);
    } else {
        // For the NULL entry, memset the descriptor to 0.
        memzero((uint8_t*)&seg,sizeof(seg));
    }

    // Perform the copy.
    memcpy((uint8_t*)&(GDT[idx]),(uint8_t*)&seg,sizeof(seg));
}

// Refresh the segment registers and use the new code and data segments.
// After this function returns:
//      CS = `code_seg`
//      DS = SS = ES = FS = GS = `data_seg`.
static void
_refresh_segment_registers(uint16_t const code_seg, uint16_t const data_seg) {
    LOG("Use segments: Code = %x, Data = %x\n",code_seg,data_seg);
    write_cs(code_seg);
    write_ds(data_seg);
    write_ss(data_seg);
    write_es(data_seg);
    write_fs(data_seg);
    write_gs(data_seg);
}

void
gdt_init(void) {
    // Add each GDT entry defined in _gdt_entries into the real GDT.
    LOG("Add %d entries to GDT ...\n",GDT_SIZE);
    for (size_t i = 0; i < GDT_SIZE; ++i) {
        struct gdt_entry const * const entry = _gdt_entries+i;
        add_gdt_entry(entry,i);
    }

    // Generate the table descriptor for the GDT.
    uint16_t const gdt_size_bytes = GDT_SIZE*sizeof(struct segment_desc_t);
    struct table_desc_t table_desc = {
        .base_addr = (uint8_t*)GDT,
        // The limit should point to the last valid bytes, thus the -1.
        .limit = gdt_size_bytes-1,
    };
    LOG("GDT base address is %p, limit = %d bytes\n",GDT,gdt_size_bytes);

    // We can now load the GDT using the table_desc_t.
    LOG("Call lgdt with address %p\n",&table_desc);
    load_gdt(&table_desc);

    // Update segment registers.
    _refresh_segment_registers(KERNEL_CODE_SEGMENT_SELECTOR,
                               KERNEL_DATA_SEGMENT_SELECTOR);
}
