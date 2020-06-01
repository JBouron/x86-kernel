#include <proc.h>
#include <frame_alloc.h>
#include <paging.h>
#include <memory.h>
#include <kmalloc.h>
#include <kernel_map.h>
#include <cpu.h>
#include <segmentation.h>

// The number of stack frames to be allocated by default for a new process.
#define DEFAULT_NUM_STACK_FRAMES    4

// Get a pointer to the bottom of a stack given a pointer to its top.
// @param stack_top: pointer to the top of the stack that is the address of the
// very last dword of the stack.
// @param num_pages: The size of the stack in number of pages.
// @return: A pointer to the bottom of the stack, that is the very first dword
// of the stack.
static void *get_stack_bottom(void const * const stack_top,
                              uint32_t const num_pages) {
    void * const ptr = (void*)stack_top;
    return ptr + num_pages * PAGE_SIZE - 4;
}

// Allocate a stack for a process in its own address space.
// @param proc: The process to allocate the stack to.
static void allocate_stack(struct proc * const proc) {
    // Allocate physical frames that will be used for the process' stack.
    uint32_t const n_stack_frames = DEFAULT_NUM_STACK_FRAMES;
    void * frames[n_stack_frames];
    for (uint32_t i = 0; i < n_stack_frames; ++i) {
        frames[i] = alloc_frame();
    }

    // Map the stack into the process' address space.
    struct addr_space * const as = proc->addr_space;
    uint32_t const map_flags = VM_WRITE | VM_USER | VM_NON_GLOBAL;
    void * const stack_top =
        paging_map_frames_above_in(as, 0x0, frames, n_stack_frames, map_flags);
    void * const stack_bottom = get_stack_bottom(stack_top, n_stack_frames);

    // Set the esp in the registers_save of the stack.
    proc->registers_save.esp = (reg_t)stack_bottom;
    proc->registers_save.ebp = proc->registers_save.esp;

    proc->stack_top = stack_top;
    proc->stack_bottom = stack_bottom;
}

struct proc *create_proc(void) {
    struct proc * const proc = kmalloc(sizeof(*proc));
    proc->addr_space = create_new_addr_space();

    // Zero out the register_save struct of the proc so that the first time it
    // gets run all registers will be 0 (except esp and ebp).
    memzero(&proc->registers_save, sizeof(proc->registers_save));

    allocate_stack(proc);

    // For processes the eflags should only have the interrupt bit set.
    proc->registers_save.eflags = 1 << 9;

    // Setup code, data and stack segments to use user space segments.
    union segment_selector_t code_seg_sel = user_code_seg_sel();
    union segment_selector_t data_seg_sel = user_data_seg_sel();
    proc->registers_save.cs = code_seg_sel.value;
    proc->registers_save.ds = data_seg_sel.value;
    proc->registers_save.es = data_seg_sel.value;
    proc->registers_save.fs = data_seg_sel.value;
    proc->registers_save.gs = data_seg_sel.value;
    proc->registers_save.ss = data_seg_sel.value;

    return proc;
}

// The following constants are used by do_far_return_to_proc() to access various
// fields of the struct register_save_area.
// This value is used by proc_asm.S to know the offset of registers_save within
// the struct proc.
uint32_t const REGISTERS_SAVE_OFFSET = offsetof(struct proc, registers_save);

// Offsets of saved registers within the struct register_save_area.
uint32_t const SS_OFF = offsetof(struct register_save_area, ss);
uint32_t const ESP_OFF = offsetof(struct register_save_area, esp);
uint32_t const EFLAGS_OFF = offsetof(struct register_save_area, eflags);
uint32_t const CS_OFF = offsetof(struct register_save_area, cs);
uint32_t const EIP_OFF = offsetof(struct register_save_area, eip);
uint32_t const DS_OFF = offsetof(struct register_save_area, ds);
uint32_t const ES_OFF = offsetof(struct register_save_area, es);
uint32_t const FS_OFF = offsetof(struct register_save_area, fs);
uint32_t const GS_OFF = offsetof(struct register_save_area, gs);

// Execute a far-return into a process. This function does not return.
// @param proc: The process to return into.
void do_far_return_to_proc(struct proc * const proc);

void switch_to_proc(struct proc * const proc) {
    switch_to_addr_space(proc->addr_space);
    do_far_return_to_proc(proc);
}

#include <proc.test>
