#include <smp.h>
#include <frame_alloc.h>
#include <lapic.h>
#include <segmentation.h>
#include <paging.h>
#include <debug.h>
#include <memory.h>
#include <acpi.h>
#include <math.h>
#include <kernel_map.h>
#include <lock.h>
#include <kmalloc.h>

// Application Processor (AP) Start Up Algorithm
// =============================================
//     This file contains the necessary logic to wake up the application
// processors in the system. On boot, only a single core/cpu is used, known as
// the Boot-Strap Processor (BSP). The APs are idling, waiting for a signal to
// wake up and start executing code. It is the responsibility of the BSP to send
// this signal and wake up APs (actually, APs can wake other APs, but the very
// first AP can only be woken up by the BSP. In this kernel the BSP is the only
// core responsible to send the wake up signal the APs).
// This comment describes the procedure adopted by this kernel to wake up the
// APs.
//
// Waking Up
// ---------
//      To wake up the APs, the BSP sends a sequence of Inter-Processor
// Interrupts (IPI) to all the APs. This sequence is: INIT - SIPI - SIPI. More
// details in the Intel's manual. The SIPIs contains the frame number of the
// code that the APs should execute. Upon receiving the second SIPI, APs will
// start executing code at physical address <vector> << 12. That is, if the BSP
// sends SIPIs with vector 0x6, the APs will start executing whatever is at
// address 0x6000 in physical memory.
// Upon wake up, APs are running in real mode. From there the procedure switches
// to protected mode and later enables paging before jumping into the higher
// half kernel. The entire code executed from wake up (in real mode) to jumping
// into the higher half kernel is implemented in ap_entry_point() in boot.S.
//
// Data-Structures
// ---------------
//      During the wake up procedure, some data structures are required. APs
// must expect at least a stack for normal operation. When switching to
// protected mode they need a GDT, finally when enabling paging they need to
// know the physical address of the page directory to be used.
// All of this information is passed through a "data frame" that resides <1MiB
// so that it is accessible from real-mode.
//
// Stack: For normal operation, APs expect a stack under the 1MiB boundary. The
// data frame contains at least 1 stack. Depending on the number of processors
// in the system and the available memory, the data frame might contain up to
// one stack per AP. This is the optimal amount. In case the current
// configuration does not allow this amount, the data frame can contain less
// stack, but some APs will have to share a stack with others. Each stack in the
// data frame is protected by a lock such that only one processor can use a
// given stack at a time.
// Stacks are stored in the stack_segments field of the data frame. Note that
// this array contains the stack segments, and not the stack addresses. There
// are `num_stacks` total in this array and processor are indexing this array
// with their APIC ID % num_stacks.
//
// GDT: The GDT used by the BSP is located above 1MiB in physical memory because
// of the relocation. Therefore, it is not possible for the AP to use it.
// Instead, the data frame contains a small GDT containing only two flat
// segments (code and data). Index 1 is the data segment while index 2 is the
// code segment. APs set up protected mode using this temporary GDT, and will
// change it at a later stage of the procedure once they can access addresses
// above 1MiB.
//
// Page Directory Addr: The data frame contains the physical address of the page
// directory they will have to use.
//
// Finding the Data Frame
// ----------------------
//     The segment containing the data frame is written at the end of the
// physical frame contain the AP wake up code.
//
// Stack Locking
// -------------
//     At the bottom of every stack is a lock that an AP _must_ acquire and hold
// while using the stack. The lock is a 16-bit word.

// ap_entry_point is the entry point function of AP cores. This is the very
// first function/instruction executed by APs at wake up.
extern void ap_entry_point(void);

// The ap_entry_point_end is placed right at the end of the ap_entry_point
// function. Its purpose is to allow us to compute the size of the
// ap_entry_point function.
extern uint8_t ap_entry_point_end;

// Test if a given physical address is under the 1MiB address.
// @param addr: The physical address to test.
// @return: true if addr points under 1MiB, false otherwise.
static bool is_under_1mib(void const * const addr) {
    return (uint32_t)addr < (1 << 20);
}

// Allocate a physical frame under 1MiB.
// @return: The physical address of the allocated frame, NULL if not frame is
// free under 1MiB.
// TODO: NULL could be a valid physical frame address, we should not rely on it
// to indicate failure. Rework this function.
static void * alloc_low_mem_stack_frame(void) {
    void * const frame = alloc_frame();
    if (is_under_1mib(frame)) {
        // This frame is under the 1MiB limit, we can use it.
        LOG("Stack frame @ %p\n", frame);
        return frame;
    } else {
        // No more physical frames available under 1MiB. Free the allocated
        // frame and return a failure.
        LOG("Couldn't find frame under 1MiB\n");
        free_frame(frame);
        return NULL;
    }
}

// Get the real-mode segment for a given address. Note: Addresses in real-mode
// are not unique eg. two different pairs of segment:offset could correspond to
// the same address. This function returns the segment for which the offset
// would be minimal.
// @param addr: The address to get the segment from.
// @return: The 16 bits segment value.
static uint16_t get_real_mode_segment_for_addr(void const * const addr) {
    return (uint32_t)addr >> 4;
}

// The size of the stack used by the APs when executing the wake up routine.
#define AP_WAKEUP_STACK_SIZE    (PAGE_SIZE / 4)

// Create a data frame containing all data structures required by the APs to
// boot.
// @return: The physical address of the data frame.
static void * create_data_frame(void) {
    void * const phy_frame = alloc_frame();

    // Since the data frame must be accessible by the APs running in real-mode
    // during their boot sequence, it must be located under the 1MiB physical
    // address.
    ASSERT(is_under_1mib(phy_frame));

    // Map the frame to virtual memory so we can copy all the data structures.
    paging_map(phy_frame, phy_frame, PAGE_SIZE, VM_WRITE);

    LOG("Data frame @ %p\n", phy_frame);

    // The data frame has its layout defined by the ap_boot_data_frame_t
    // structure.
    struct ap_boot_data_frame_t * const data_frame = phy_frame;

    // Initialize the flat code and data segments in the temporary GDT located
    // in the data frame that will be used by the APs when switching to
    // protected mode.
    initialize_trampoline_gdt(data_frame);

    // Put the page directory physical address in the data frame to allow APs to
    // enable paging.
    data_frame->page_dir_addr = get_kernel_page_dir_phy_addr();

    // ACPI tables gives us the number of cpus present in the system.
    uint16_t const ncpus = acpi_get_number_cpus();
    // Since the BSP is also listed in the ACPI tables this is the number of
    // expected APs:
    uint16_t const naps = ncpus - 1;

    // APs do not need a big stack during boot/initialization. Since we are
    // limited in the number of available physical frames under 1MiB, allocate
    // multiple stacks per frame.
    uint32_t const stack_size = AP_WAKEUP_STACK_SIZE;
    ASSERT(!(PAGE_SIZE % stack_size));
    data_frame->stack_size = stack_size;
    uint8_t const stacks_per_frame = PAGE_SIZE / stack_size;

    LOG("%u cpus on system.\n", ncpus);

    // Compute the number of physical frames required for the optimal number of
    // stacks. Note: In reality, depending on the amount of available physical
    // frames under 1MiB, we might not be able to allocate that many frames. If
    // this is the case, APs will have to share stacks.
    uint32_t const nframes = ceil_x_over_y_u32(naps * stack_size, PAGE_SIZE);
    LOG("Using %u stack frames for APs\n", nframes);

    data_frame->num_stacks = 0;

    // Try to allocate `nframes` physical frames.
    for (uint32_t i = 0; i < nframes; ++i) {
        // Try to allocate a new stack frame under 1MiB.
        void * const frame = alloc_low_mem_stack_frame();

        if (!frame) {
            // There are no physical frame left under the 1MiB limit. Some cpus
            // will have to share a stack frame. This is ok since each stack
            // frame has a lock specifically added for this case.
            break;
        }

        // Map the frame for two reasons:
        //  1. We want to initialize the stack (eg. the lock).
        //  2. When APs will enable paging, they will need their stack mapped to
        //  virtual memory.
        paging_map(frame, frame, PAGE_SIZE, VM_WRITE);

        // Initialize the stacks contained in this physical frame.
        for (uint8_t j = 0; j < stacks_per_frame; ++j) {
            // Compute the addres of the top of the stack.
            void * const stack = frame + j * stack_size;

            // Initialize the lock for this stack. This lock is the very first
            // word at the bottom of the stack.
            void * const stack_bottom = stack + stack_size - 1;
            // Set the lock to unlocked.
            *(uint8_t*)stack_bottom = 0;

            // Insert the real-mode segment corresponding to this stack into the
            // stack_segments array.
            uint32_t const index = i * stacks_per_frame + j;
            uint16_t const segment = get_real_mode_segment_for_addr(stack);
            data_frame->stack_segments[index] = segment;
            data_frame->num_stacks ++;

            LOG("stack[%u] = %p\n", index, stack);

            if (index + 1 >= naps) {
                // We have enough stacks to go on. Stop now.
                break;
            }
        }
    }
    
    // There should be at least one stack available for the APs to use.
    ASSERT(data_frame->num_stacks);
    LOG("Using %u stacks for %u cpus\n", data_frame->num_stacks, naps);
    return phy_frame;
}

// Write the data segment to use an the end of the code physical frame.
// @param code_frame: The physical address of the frame containing the APs boot
// code.
// @param data_segment: The segment value to write at the end of code_frame.
static void insert_data_segment_in_frame(void * const code_frame,
                                         uint16_t const data_segment) {
    // Write data_segment in the last word of the code frame.
    uint16_t * const last_word_addr = (uint16_t*)(code_frame + PAGE_SIZE) - 1;
    *last_word_addr = (uint16_t)(uint32_t)data_segment;
}

// Extract the physical address of the data frame from the code frame. This
// address is computed from the real-mode data segment stored in the last word
// of code frame.
// @param code_frame: The code frame to extract the address from.
// @return: The physical address of the data frame.
static void *get_data_frame_addr_from_frame(void * const code_frame) {
    uint16_t * const last = (uint16_t*)(code_frame + PAGE_SIZE) - 1;
    // Since the segment is encoded at the end of the code frame we need to
    // translate it into a physical address as real-mode does.
    return (void*)(*last << 4);
}

// Create the trampoline, that is the boot code, data frames and stacks
// necessarty to boot and initialize the APs all the way to the higher-half
// kernel.
// @return: The physical address of the entry point for the APs.
static void * create_trampoline(void) {
    void * const code_frame = alloc_frame();
    LOG("Trampoline code frame at %p\n", code_frame);

    // With the code frame used by APs we have a tighter requirement. Indeed we
    // need to make a far jump when switching from real-mode to protected mode,
    // which requires a far pointer composed of a 2 bytes segment and 2 bytes
    // offset. This 2 bytes offset is what limits us here. It forces us to have
    // the target of the far jump under 65K. Therefore we need the following
    // assert:
    ASSERT((uint32_t)code_frame < (1 << 16));

    // We need to ID map the code frame for two reasons:
    //  _ First, we need to copy the AP startup code to the freshly allocated
    //  physical frame.
    //  _ Second, when the APs will enable paging on their side, they will be
    //  running on the physical code frame.
    paging_map(code_frame, code_frame, PAGE_SIZE, VM_WRITE);

    // Copy the startup code onto the physical frame. All the code necessary to
    // handle AP boot is between the ap_entry_point and ap_entry_point_end
    // labels. Currently, we only support a boot code of up to 4KiB, the size of
    // a page.  Since the boot code is small, it should be more than enough.
    size_t const len = (void*)&ap_entry_point_end - (void*)ap_entry_point;
    ASSERT(len < PAGE_SIZE);
    memcpy(code_frame, ap_entry_point, len);

    // Setup the data frame that will contain all the necessary data structures
    // for AP booting.
    void const * const data_frame = create_data_frame();

    // Compute the data segment that APs should use in real-mode addressing and
    // insert it at the end of the code frame. Opon receiving the SIPI(s) the
    // APs will be able to retrieve the value of the data segment to use by
    // reading at address 0xFFE in the code segment.
    uint16_t const data_segment = get_real_mode_segment_for_addr(data_frame);
    insert_data_segment_in_frame(code_frame, data_segment);

    // Since this is a code frame we should ideally make it read only after
    // copying the code on it. The current paging interface forces us to unmap
    // and remap the page.
    paging_unmap(code_frame, PAGE_SIZE);
    paging_map(code_frame, code_frame, PAGE_SIZE, 0);

    return code_frame;
}

// Clean up all allocated memory (physical and virtual) during AP wake up
// routine, that is:
//  _ The physical frame containing the wake up code.
//  _ Any physical frame used for temporary AP stacks.
//  _ The physical frame used as the data frame.
// The virtual mappings for all of the above frame is removed as well.
// @param code: The physical address of the frame containing the AP wake up
// code.
static void cleanup_ap_wakeup_routine_allocs(void * const code_frame) {
    struct ap_boot_data_frame_t * const data_frame =
        get_data_frame_addr_from_frame(code_frame);

    // Unmap the frame containing the AP wake up code.
    paging_unmap(code_frame, PAGE_SIZE);
    free_frame(code_frame);

    // Iterate over the stack and de-allocate them.
    uint32_t const inc = PAGE_SIZE / AP_WAKEUP_STACK_SIZE;
    for (uint32_t i = 0; i < data_frame->num_stacks; i += inc) {
        void * const stack_frame = (void*)(data_frame->stack_segments[i] << 4);
        paging_unmap(stack_frame, PAGE_SIZE);
        free_frame(stack_frame);
    }

    // De-allocate the data frame.
    paging_unmap(data_frame, PAGE_SIZE);
    free_frame(data_frame);
}

// Upon waking, APs are changing the state of the kernel through stack
// allocations, logging, ... All of this needs to be protected against race
// conditions.
// As this is initialization stage we can use a big global lock without much
// performance penalties.
// The AP_BOOT_LOCK should be held when modifying the kernel state in anyway.
// It is only meant to prevent race condition while APs are waking up, as the
// BSP is waiting in the meantime.
DECLARE_SPINLOCK(AP_BOOT_LOCK);

// The number of Application Processor that are fully woken up and initialized.
// This function serves as a signal to the BSP that all APs are ready. It must
// be incremented _once_ per AP, while holding the AP_BOOT_LOCK.
static uint8_t APS_READY = 0;

// This is kind of a hacky way to do compute the size of the kernel stack: Use
// the same as the BSP's.
extern uint8_t stack_top;
extern uint8_t stack_bottom;
#define KERNEL_STACK_SIZE   ((size_t)(&stack_top - &stack_bottom))

// Allocate a stack in the higher half kernel of size KERNEL_STACK_SIZE.
// @return: The address of the top of the stack.
void *ap_alloc_higher_half_stack(void) {
    // As this function is going to change global state (i.e. page tables and
    // physical frames allocations) we need to acquire the AP_BOOT_LOCK. In the
    // meantime, the BSP is waiting for the APs to boot and therefore is not
    // accessing any of this states.
    spinlock_lock(&AP_BOOT_LOCK);

    // Find a big enough hole in the virtual address space to fit the kernel
    // stack for this cpu.
    size_t const size = ceil_x_over_y_u32(KERNEL_STACK_SIZE, PAGE_SIZE);
    void * const vaddr = paging_find_contiguous_non_mapped_pages(
        KERNEL_PHY_OFFSET, size);
    LOG("Found stack @ %p\n", vaddr);

    // Allocate physical frames for the stack and map them to the higher half
    // kernel.
    for (uint32_t i = 0; i < size; ++i) {
        void * const frame = alloc_frame();
        paging_map(frame, vaddr + i * PAGE_SIZE, PAGE_SIZE, VM_WRITE);
    }

    // Allocation is over let other cpus do theirs.
    spinlock_unlock(&AP_BOOT_LOCK);

    // Return the virtual address of the top of the stack.
    return vaddr;
}

void ap_finalize_start_up(void) {
    // This AP has a private stack in higher half that is of a decent size.
    // Before being fully operational, a few operations need to be done one this
    // cpu. The next few functions do not require the AP_BOOT_LOCK has they are
    // setting cpu-private states.
    
    // Start by enabling the cache.
    cpu_enable_cache();

    // This AP is still using the temporary GDT from the ap wake up procedure at
    // this point. Switch to the real GDT.
    ap_init_segmentation();

    // Initialize interrupts on this AP as well as the LAPIC.
    ap_interrupt_init();
    ap_init_lapic();

    uint8_t const apic_id = cpu_apic_id();

    // Log that the CPU is ready and inc the APS_READY to report back to the
    // BSP.
    spinlock_lock(&AP_BOOT_LOCK);
    LOG("CPU %u ready with stack %p\n", apic_id, cpu_read_esp());
    APS_READY ++;

    spinlock_unlock(&AP_BOOT_LOCK);

    // Start a loop waiting for interrupts. This will be a useful starting
    // point for future development.
    while (true) {
        cpu_set_interrupt_flag(true);
        cpu_halt();
    }
}

void init_aps(void) {
    // Create the trampoline.
    void * const ap_entry_point = create_trampoline();

    // FIXME: The boot code will make a call to cpu_enable_paging_bits to enable
    // paging. Therefore this function must be ID mapped into virtual memory.
    extern void cpu_enable_paging_bits(void);
    void const * const func_addr = to_phys(cpu_enable_paging_bits);
    paging_map(func_addr, func_addr, PAGE_SIZE, 0);

    // The Intel manual provide the following algorithm to boot APs (manual
    // volume 3, chapter 8.4.4.1):
    //  1. Send a broadcast INIT IPI to all APs.
    //  2. Wait 10ms.
    //  3. Send a broadcast SIPI to all APs.
    //  4. Wait 200 microsecond.
    //  5. Send a second broadcast SIPI.
    //  6. Wait for all the APs to announce themselves one way or another.
    // It turns out that most of the time (actually all the time in Qemu) the
    // second SIPI is not necessary as the APs wake up on the first SIPI.
    // Nevertheless it is still recommended to follow this INIT-SIPI-SIPI
    // sequence for portability and to avoid eventual undefined behaviour.

    lapic_send_broadcast_init();
    lapic_sleep(10);

    lapic_send_broadcast_sipi(ap_entry_point);
    // FIXME: The lapic sleep only has a millisecond granularity. Therefore wait
    // 1ms instead of 200 micro-second. That should be fine.
    lapic_sleep(1);

    lapic_send_broadcast_sipi(ap_entry_point);
    lapic_sleep(200);

    // Now wait for all the Application Processors to wake up and initialize.
    uint8_t const naps = acpi_get_number_cpus() - 1;
    while (APS_READY != naps) {
        lapic_sleep(100);
    }

    LOG("All APs ready!\n");

    // All APs are woken up and all have allocated their private stack. We can
    // now clean up the code frame containing the wake up routine, the data
    // frame and the stack frames.
    cleanup_ap_wakeup_routine_allocs(ap_entry_point);
    paging_unmap(func_addr, PAGE_SIZE);
}

#include <smp.test>