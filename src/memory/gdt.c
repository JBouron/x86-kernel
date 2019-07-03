#include <memory/gdt.h>
#include <memory/segment.h>
#include <asm/asm.h>
#include <memory/memory.h>
#include <includes/debug.h>

// Privilege levels.
#define PRIV_RING0  (0)
#define PRIV_RING3  (3)

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

// We use specific segments for the kernel.
uint16_t const KERNEL_CODE_SEGMENT_IDX = 1;
uint16_t const KERNEL_DATA_SEGMENT_IDX = 2;

// The static list of GDT entries we wish to add to the GDT.
static struct gdt_entry _gdt_entries[] = {
    {0,0,0,0}, // The null segment. It must be present as the first entry.
    {0x0,0xFFFFF,PRIV_RING0,0xA}, // Kernel code segment. 0xA = Code/Executable.
    {0x0,0xFFFFF,PRIV_RING0,0x2}, // Kernel data segment. 0x2 = Data/Writable.
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
    uint16_t const kernel_code_seg = KERNEL_CODE_SEGMENT_IDX<<3|PRIV_RING0;
    uint16_t const kernel_data_seg = KERNEL_DATA_SEGMENT_IDX<<3|PRIV_RING0;
    _refresh_segment_registers(kernel_code_seg,kernel_data_seg);
}
