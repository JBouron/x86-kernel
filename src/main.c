#include <types.h>
#include <vga.h>
#include <tty.h>
#include <string.h>
#include <cpu.h>
#include <debug.h>
#include <cpuid.h>
#include <multiboot.h>
#include <memory.h>
#include <math.h>
#include <test.h>
#include <serial.h>
#include <segmentation.h>
#include <interrupt.h>
#include <lapic.h>
#include <bitmap.h>
#include <frame_alloc.h>
#include <paging.h>
#include <kernel_map.h>
#include <multiboot.h>
#include <list.h>
#include <kmalloc.h>
#include <acpi.h>
#include <ioapic.h>

// Execute all the tests in the kernel.
static void test_kernel(void) {
    LOG("Running tests:\n");
    vga_test();
    mem_test();
    str_test();
    math_test();
    tty_test();
    cpu_test();
    serial_test();
    segmentation_test();
    interrupt_test();
    // The lapic test is disabled for now as it generate spurious interrupts
    // that cause panics.
    //lapic_test();
    bitmap_test();
    frame_alloc_test();
    paging_test();
    multiboot_test();
    list_test();
    kmalloc_test();
    lapic_test();
    ioapic_test();
    print_test_summary();
}

static void pretty_print_mutliboot_hdr(struct multiboot_info const * hdr) {
    LOG("Memory low = %x\n", hdr->mem_lower * 1024);
    LOG("Memory hi  = %x\n", hdr->mem_upper * 1024);
    LOG("Mmap len  = %u\n", hdr->mmap_length);
    LOG("Mmap addr = %x\n", hdr->mmap_addr);

    uint64_t tot_mem = 0;
    void const * const addr = to_virt((void*)hdr->mmap_addr);
    struct multiboot_mmap_entry const * entry = (struct multiboot_mmap_entry*)addr;
    while (entry < (struct multiboot_mmap_entry*)(addr + hdr->mmap_length)) {
        uint64_t const start = entry->base_addr;
        uint64_t const end = start + entry->length - 1;
        char const * const type = entry->type == 1 ? "Available" : "Reserved ";
        LOG("%X - %X => %s (%X)\n", start, end, type, entry->length);
        entry ++;
        if (entry->type == 1) {
            tot_mem += entry->length;
        }
    }
    LOG("Total memory %U bytes\n", tot_mem);
    LOG("Kernel is    %u bytes\n", KERNEL_SIZE);
    LOG(".text  is    %u bytes\n", SECTION_TEXT_SIZE);
    LOG(".rodata  is  %u bytes\n", SECTION_RODATA_SIZE);
    LOG(".data  is    %u bytes\n", SECTION_DATA_SIZE);
    LOG(".bss  is     %u bytes\n", SECTION_BSS_SIZE);
}

// Trigger a shutdown of the virtual machine. This function has an undefined
// behaviour for bare-metal installations.
static void virt_shutdown(void) {
    // With QEMU writing 0x2000 into the port 0x604 triggers the shutdown.
    cpu_outw(0x604, 0x2000);
}

#define id_map(addr) \
    paging_map(addr, addr, PAGE_SIZE, 0)

__attribute__((unused)) static void pagefault_id_map_handler(struct interrupt_frame_t const * const frame) {
    LOG("####### PAGE FAULT ######\n");
    LOG("EIP = %p\n", frame->eip);
    void * const cr2 = cpu_read_cr2();
    id_map(get_page_addr(cr2));
    ASSERT(frame);
}

static void serial_interrupt_handler(struct interrupt_frame_t const * const frame) {
    ASSERT(frame);
    char c = '\0';
    if (SERIAL_STREAM.read((uint8_t*)(&c), 1)) {
        LOG("%c", c);
    }
}

void
kernel_main(struct multiboot_info const * const multiboot_info) {
    ASSERT(multiboot_info);
    vga_init();
    serial_init();
    tty_init(NULL, &SERIAL_STREAM);

    LOG("\n");
    LOG("=== ### ===\n");
    LOG("Kernel loaded at %p-%p\n", to_phys(KERNEL_START_ADDR), to_phys(KERNEL_END_ADDR));
    LOG("Kernel size: %x bytes\n", KERNEL_SIZE);

    acpi_init();
    interrupt_init();
    init_lapic();
    init_ioapic();
    cpu_set_interrupt_flag(true);

    uint32_t const start_alloc_frames = frames_allocated();
    test_kernel();

    LOG("Multiboot header at %p\n", multiboot_info);
    struct multiboot_info const * const hdr = to_virt(multiboot_info);
    pretty_print_mutliboot_hdr(hdr);

    uint32_t const end_alloc_frames = frames_allocated();
    uint32_t const delta_alloc_frames = end_alloc_frames - start_alloc_frames;
    LOG("%u frames allocated\n", delta_alloc_frames);
    void * addr = kmalloc(234);
    LOG("Alloc at %p\n", addr);

    interrupt_register_callback(32, serial_interrupt_handler);

    redirect_isa_interrupt(4, 32);
    cpu_set_interrupt_flag(true);

    while(1);
    virt_shutdown();
}
