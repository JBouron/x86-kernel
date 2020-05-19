#pragma once
#include <types.h>
#include <cpu.h>
#include <debug.h>

// This is the layout of the data frame used by APs to boot. This data frame
// contains all the information and data structures required by an AP to boot
// into the higher half kernel.
struct ap_boot_data_frame_t {
    // Instead of letting the APs to the computation of the GDT table
    // descriptor, let the BSP do it and store it into the data frame. This way
    // the APs only have to execute an lgdt with a pointer to this field.
    struct gdt_desc_t gdt_desc;
    
    // APs are using a simple, temporary GDT when going into protected mode.
    // This GDT contains two flat segments: A data segment (index 1) and a code
    // segment (index 2).
    uint64_t gdt[3];

    // APs will need the physical address of the kernel's page directory upon
    // enabling paging.
    void const * page_dir_addr;

    // The function the APs should call once they are fully initialized.
    void (*wake_up_target)(void);

    // When initializing their stack pointers, APs need the size of their stack.
    uint16_t stack_size;

    // The number of entries available in the stack_segments array.
    uint32_t num_stacks;

    // This array contains the stack segments of all the available stacks that
    // APs can use. When choosing a stack segment (and therefore a stack) APs
    // will index this array with their temporary AP ID % num_stacks.
    uint16_t stack_segments[0];
} __attribute__((packed));

// Since we need to use hardcoded values for the offset in the ap start up
// routine, make sure that the offset are as expected. In case the change, the
// assert(s) below would fail and give a hint that the assembly needs to change
// as well.
STATIC_ASSERT(offsetof(struct ap_boot_data_frame_t, gdt_desc) == 0, "");
STATIC_ASSERT(offsetof(struct ap_boot_data_frame_t, page_dir_addr) == 0x1E, "");
STATIC_ASSERT(offsetof(struct ap_boot_data_frame_t, wake_up_target) == 0x22, "");
STATIC_ASSERT(offsetof(struct ap_boot_data_frame_t, stack_size) == 0x26, "");
STATIC_ASSERT(offsetof(struct ap_boot_data_frame_t, num_stacks) == 0x28, "");
STATIC_ASSERT(offsetof(struct ap_boot_data_frame_t, stack_segments) == 0x2C,"");

// Wake up the Application Processors on the system and wait for them to be
// fully initialized.
void init_aps(void);

// Returns whether or not Application Processors are currently online i.e.
// running.
// @return: true if the APs are online, false otherwise.
bool aps_are_online(void);

// Execute tests related to smp.
void smp_test(void);
