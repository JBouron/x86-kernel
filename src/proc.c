#include <proc.h>
#include <frame_alloc.h>
#include <paging.h>
#include <memory.h>
#include <kmalloc.h>
#include <kernel_map.h>
#include <cpu.h>

// This value is used by proc_asm.S to know the offset of registers_save within
// the struct proc.
uint32_t const REGISTERS_SAVE_OFFSET = (offsetof(struct proc, registers_save));
// The offset of the saved value of ESP register within a struct proc.
uint32_t const ESP_OFFSET =
    REGISTERS_SAVE_OFFSET + (offsetof(struct register_save_area, esp));

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
    void * const stack_top =
        paging_map_frames_above_in(as, 0x0, frames, n_stack_frames, VM_WRITE);
    void * const stack_bottom = get_stack_bottom(stack_top, n_stack_frames);

    // Set the esp in the registers_save of the stack.
    proc->registers_save.esp = (reg_t)stack_bottom;
    proc->registers_save.ebp = proc->registers_save.esp;

    proc->stack_top = stack_top;
    proc->stack_bottom = stack_bottom;
}

// Setup the stack of a new process to contain the initial far-return that will
// be used for the first execution of the process.
// @param proc: The process to setup.
static void prepare_initial_far_return(struct proc * const proc) {
    // The stack should contain the following:
    //    | EFLAGS |
    //    | CS     |
    //    | EIP    | <- ESP
    // Since we don't have a user space in ring 3 for now, the value of CS
    // should be the CS for the kernel code.
    // Note: It is easier here to temporarily switch to the process' address
    // space to modify its stack. The main reason being that mapping its stack
    // to the current address space would be hard because we don't know which
    // physical frames we should map (struct proc only tells us the top frame
    // but not the other ones).
    switch_to_addr_space(proc->addr_space);

    uint32_t * stack_ptr = (void*)proc->registers_save.esp;
    *stack_ptr = (uint32_t)proc->registers_save.eflags;
    stack_ptr --;
    *stack_ptr = (uint32_t)cpu_read_cs().value;
    stack_ptr --;
    *stack_ptr = (uint32_t)proc->registers_save.eip;

    switch_to_addr_space(get_kernel_addr_space());

    // Fix up the ESP register of the process to point to the new top of the
    // stack.
    proc->registers_save.esp = (reg_t)stack_ptr;
}

struct proc *create_proc(void const * const eip) {
    struct proc * const proc = kmalloc(sizeof(*proc));
    proc->addr_space = create_new_addr_space();

    // Zero out the register_save struct of the proc so that the first time it
    // gets run all registers will be 0 (except esp and ebp).
    memzero(&proc->registers_save, sizeof(proc->registers_save));

    allocate_stack(proc);

    // For processes the eflags should only have the interrupt bit set.
    proc->registers_save.eflags = 1 << 9;
    proc->registers_save.eip = (reg_t)eip;

    // Prepare the far return that will be used to start the first execution of
    // the process.
    prepare_initial_far_return(proc);

    return proc;
}

// Execute a far-return into a process. The process' stack should already
// contain the correct data to execute the return either by calling
// prepare_initial_far_return() or if the process has been interrupted.
// @param proc: The process to return into.
void do_far_return_to_proc(struct proc * const proc);

void switch_to_proc(struct proc * const proc) {
    switch_to_addr_space(proc->addr_space);
    do_far_return_to_proc(proc);
}

#include <proc.test>
