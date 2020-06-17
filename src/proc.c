#include <proc.h>
#include <frame_alloc.h>
#include <paging.h>
#include <memory.h>
#include <kmalloc.h>
#include <kernel_map.h>
#include <cpu.h>
#include <segmentation.h>
#include <sched.h>
#include <vfs.h>

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
    uint32_t map_flags = VM_WRITE | VM_NON_GLOBAL;
    if (!proc->is_kernel_proc) {
        map_flags |= VM_USER;
    }
    // When mapping stacks for kernel processes, we avoid mapping under the 1MiB
    // addresses. This is because those addresses are used by SMP and/or BIOS
    // and sometimes need to be identically mapped.
    void * const low = (void*)(proc->is_kernel_proc ? 0x100000 : 0x0);
    void * const stack_top =
        paging_map_frames_above_in(as, low, frames, n_stack_frames, map_flags);
    void * const stack_bottom = get_stack_bottom(stack_top, n_stack_frames);

    // Set the esp in the registers_save of the stack.
    proc->registers_save.esp = (reg_t)stack_bottom;
    proc->registers_save.ebp = proc->registers_save.esp;

    proc->stack_top = stack_top;
    proc->stack_bottom = stack_bottom;
    proc->stack_pages = n_stack_frames;
}

// This lock is used by get_new_pid() to atomically get a new pid.
DECLARE_SPINLOCK(PID_LOCK);
// The next pid to be generated. Must be accessed while holding PID_LOCK;
static pid_t NEXT_PID = 0;

// Generate a new PID.
// @return: New unique PID.
static pid_t get_new_pid(void) {
    spinlock_lock(&PID_LOCK);
    pid_t const pid = NEXT_PID++;
    spinlock_unlock(&PID_LOCK);
    return pid;
}

static struct proc *create_proc_in_ring(uint8_t const ring) {
    ASSERT(ring == 0 || ring == 3);
    struct proc * const proc = kmalloc(sizeof(*proc));
    proc->is_kernel_proc = (ring == 0);

    // Kernel processes use the kernel address space.
    proc->addr_space =
        (ring == 0) ? get_kernel_addr_space() : create_new_addr_space();

    // Zero out the register_save struct of the proc so that the first time it
    // gets run all registers will be 0 (except esp and ebp).
    memzero(&proc->registers_save, sizeof(proc->registers_save));

    allocate_stack(proc);

    // For processes the eflags should only have the interrupt bit set.
    proc->registers_save.eflags = 1 << 9;

    // Setup code, data and stack segments to use user space segments.
    union segment_selector_t cs =
        (ring == 0) ? kernel_code_selector() : user_code_seg_sel();
    union segment_selector_t ds =
        (ring == 0) ? kernel_data_selector() : user_data_seg_sel();
    proc->registers_save.cs = cs.value;
    proc->registers_save.ds = ds.value;
    proc->registers_save.es = ds.value;
    proc->registers_save.fs = ds.value;
    // If the task is to be run in ring 0 give it access to percpu vars through
    // GS.
    proc->registers_save.gs = (ring == 0) ? cpu_read_gs().value : ds.value;
    proc->registers_save.ss = ds.value;

    list_init(&proc->rq);

    // A process is not runnable until its EIP is setup.
    proc->state_flags = PROC_WAITING_EIP;

    proc->pid = get_new_pid();

    return proc;
}

struct proc *create_proc(void) {
    return create_proc_in_ring(3);
}

struct proc *create_kproc(void (*func)(void*), void * const arg) {
    struct proc * const kproc = create_proc_in_ring(0);
    // Put the argument on the stack of the kproc. Note: stack_bottom points to
    // the first _valid_ dword on the stack. We can directly access the stack
    // here as the address space of the kproc is the kernel address space.
    ASSERT(get_curr_addr_space() == get_kernel_addr_space());
    ASSERT((void*)kproc->registers_save.esp == kproc->stack_bottom);
    *(void **)kproc->registers_save.esp = arg;
    // Since a kernel process is analoguous to a function call, we need to push
    // a return onto the stack. Put a dummy address.
    kproc->registers_save.esp -= 4;
    *(void **)kproc->registers_save.esp = NULL;

    // Setup the EIP to point to the function to be executed.
    kproc->registers_save.eip = (reg_t)(void*)func;

    // Kernel processes are runnable as soon as they are created since this
    // function sets up the EIP.
    kproc->state_flags = PROC_RUNNABLE;

    return kproc;
}

// The following constants are used by do_far_return_to_proc() to access various
// fields of the struct register_save_area.
// This value is used by proc_asm.S to know the offset of registers_save within
// the struct proc.
uint32_t const REGISTERS_SAVE_OFFSET = offsetof(struct proc, registers_save);
uint32_t const IS_KPROC_OFFSET = offsetof(struct proc, is_kernel_proc);

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
    this_cpu_var(curr_proc) = proc;
    switch_to_addr_space(proc->addr_space);
    do_far_return_to_proc(proc);
}

void delete_proc(struct proc * const proc) {
    if (proc->is_kernel_proc) {
        // Kernel processes use the kernel address space as their address space,
        // therefore it should not be deleted.
        // However, their stack should be de-allocated.
        void * const stack = proc->stack_top;
        ASSERT(proc->stack_bottom > proc->stack_top);
        ASSERT(get_curr_addr_space() == get_kernel_addr_space());
        paging_unmap_and_free_frames(stack, proc->stack_pages * PAGE_SIZE);
    } else {
        delete_addr_space(proc->addr_space);
    }
    kfree(proc);
}

#include <proc.test>
