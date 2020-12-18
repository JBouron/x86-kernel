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
#include <error.h>

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

// De-allocate a stack.
// @param stack: The struct stack describing the stack to be de-allocated.
static void dealloc_stack(struct stack const * const stack) {
    void * const top = stack->top;
    ASSERT(stack->bottom > stack->top);
    // De-allocating a stack must be done from the kernel address space.
    ASSERT(get_curr_addr_space() == get_kernel_addr_space());
    paging_unmap_and_free_frames(top, stack->num_pages * PAGE_SIZE);
}

// Allocate a stack for a process in its own address space.
// @param proc: The process to allocate the stack to.
// @param kernel_stack: If true, the kernel stack of the process will be
// allocated, otherwise it is the user stack.
// @return: true if the stack was successfully created, false otherwise.
static bool allocate_stack(struct proc * const proc, bool const kernel_stack) {
    // Kernel processes do not have a user stack, only a kernel stack.
    ASSERT(!proc->is_kernel_proc || kernel_stack);

    // Allocate physical frames that will be used for the process' stack.
    uint32_t const n_stack_frames = DEFAULT_NUM_STACK_FRAMES;
    void * frames[n_stack_frames];
    for (uint32_t i = 0; i < n_stack_frames; ++i) {
        frames[i] = alloc_frame();
        if (frames[i] == NO_FRAME) {
            for (uint32_t j = 0; j < i; ++j) {
                free_frame(frames[j]);
            }
            SET_ERROR("Could not allocate frame for process stack", ENONE);
            return false;
        }
    }

    // Map the stack into the process' address space.
    uint32_t map_flags = VM_WRITE | VM_NON_GLOBAL;
    if (!kernel_stack) {
        map_flags |= VM_USER;
    }
    // When mapping stacks for kernel processes, we avoid mapping under the 1MiB
    // addresses. This is because those addresses are used by SMP and/or BIOS
    // and sometimes need to be identically mapped.
    void * const low = (void*)(kernel_stack ? KERNEL_PHY_OFFSET : 0x0);

    // When mapping kernel stacks in kernel address space, we need to operate on
    // the kernel address space.
    struct addr_space * const as =
        kernel_stack ? get_kernel_addr_space() : proc->addr_space;
    void * const stack_top =
        paging_map_frames_above_in(as, low, frames, n_stack_frames, map_flags);
    if (stack_top == NO_REGION) {
        SET_ERROR("Could not map process' stack to its addr space", ENONE);
        for (uint32_t i = 0; i < n_stack_frames; ++i) {
            free_frame(frames[i]);
        }
        return false;
    }

    void * const stack_bottom = get_stack_bottom(stack_top, n_stack_frames);
    if (kernel_stack) {
        proc->kernel_stack.top = stack_top;
        proc->kernel_stack.bottom = stack_bottom;
        proc->kernel_stack.num_pages = n_stack_frames;
    } else {
        proc->user_stack.top = stack_top;
        proc->user_stack.bottom = stack_bottom;
        proc->user_stack.num_pages = n_stack_frames;
    }
    return true;
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

// Initialize the registers for a process.
// @param proc: The process to initialize the registers for.
static void init_registers(struct proc * const proc) {
    bool const kproc = proc->is_kernel_proc;
    struct register_save_area * const regs = &proc->registers;

    // Zero out all the registers. EAX, EBX, ECX, EDX, EDI and ESI will be 0 at
    // the first execution of the process.
    memzero(regs, sizeof(*regs));

    // Set the ESP to point to the correct stack (user or kernel) and create the
    // original stack frame with EBP.
    if (kproc) {
        // Kernel processes use their kernel stack as their "application" stack.
        regs->esp = (reg_t)proc->kernel_stack.bottom;
    } else {
        // For user processes this is the bottom of the user stack.
        regs->esp = (reg_t)proc->user_stack.bottom;
    }
    regs->ebp = regs->esp;

    // For processes the EFLAGS should only have the interrupt bit set.
    regs->eflags = 1 << 9;

    // Setup code, data and stack segments to user or kernel segments.
    union segment_selector_t const cs =
        (kproc) ? kernel_code_selector() : user_code_seg_sel();
    union segment_selector_t const ds =
        (kproc) ? kernel_data_selector() : user_data_seg_sel();
    regs->cs = cs.value;
    regs->ds = ds.value;
    regs->es = ds.value;
    regs->fs = ds.value;
    // If the task is to be run in ring 0 give it access to percpu vars through
    // GS. This is actually not needed, the function resuming the execution of a
    // process will make sure that the correct GS is loaded and will ignore the
    // value of the saved GS (except for user processes).
    regs->gs = (kproc) ? cpu_read_gs().value : ds.value;
    regs->ss = ds.value;
}

// Create a new process. This function will set default values for the process'
// registers and allocate a stack.
// @param ring: The privilege level of the process.
// @return: On success return a pointer on the initialized struct proc,
// otherwise NULL is returned.
static struct proc *create_proc_in_ring(uint8_t const ring) {
    ASSERT(ring == 0 || ring == 3);
    struct proc * const proc = kmalloc(sizeof(*proc));
    if (!proc) {
        SET_ERROR("Could not allocate struct proc", ENONE);
        return NULL;
    }
    proc->is_kernel_proc = (!ring);

    // Kernel processes use the kernel address space whereas user processes will
    // have their own address space.
    proc->addr_space =
        (!ring) ? get_kernel_addr_space() : create_new_addr_space();

    if (!proc->addr_space) {
        SET_ERROR("Cannot create address space for new process", ENONE);
        kfree(proc);
        return NULL;
    }

    // User processes will have a user stack besides the kernel stack. Kernel
    // processes only have a kernel stack.
    if (!proc->is_kernel_proc && !allocate_stack(proc, false)) {
        SET_ERROR("Could not allocate user stack for process", ENONE);
        delete_addr_space(proc->addr_space);
        kfree(proc);
        return NULL;
    }

    // Allocate kernel stack for the process.
    if (!allocate_stack(proc, true)) {
        SET_ERROR("Could not allocate kernel stack for process", ENONE);
        // De-allocate the user stack.
        paging_unmap_and_free_frames(proc->user_stack.top,
                                     proc->user_stack.num_pages * PAGE_SIZE);
        if (ring) {
            delete_addr_space(proc->addr_space);
        }
        kfree(proc);
        return NULL;
    }

    // Initialize registers to their default values. This must be done after
    // allocating the stack(s) since ESP and EBP will be pointing onto the
    // kernel/user stack.
    init_registers(proc);

    list_init(&proc->rq);

    // A process is not runnable until its EIP is setup. This function will not
    // set the EIP.
    proc->state_flags = PROC_WAITING_EIP;

    proc->pid = get_new_pid();

    // All other fields are 0 since the kmalloc memzeroed the struct proc for
    // us.

    return proc;
}

struct proc *create_proc(void) {
    // For user processes, the create_proc_in_ring() will do all the required
    // work. After this call one only needs to copy the code into the process'
    // address space and point EIP to the right place.
    return create_proc_in_ring(3);
}

// This function is meant to be called if a kernel stack underflow occurs while
// context switching. This is a protection mechanism used when a function passed
// to init_kernel_stack returns.
static void catch_kstack_underflow(void) {
    PANIC("Kernel stack underflow");
}

struct proc *create_kproc(void (*func)(void*), void * const arg) {
    struct proc * const kproc = create_proc_in_ring(0);
    if (!kproc) {
        return NULL;
    }

    // For kernel procs, we need to build a stack frame for the function call to
    // `func`. More specifically, we need to push in that order:
    //  - arg
    //  - Dummy return address. Use catch_kstack_underflow so that any return
    //  would be caught and PANIC.
    uint32_t * esp = (uint32_t*)kproc->registers.esp;
    *(--esp) = (uint32_t)arg;
    *(--esp) = (uint32_t)&catch_kstack_underflow;
    kproc->registers.esp = (reg_t)esp;

    // Set the EIP to the target function.
    kproc->registers.eip = (reg_t)func;

    // Kernel processes are runnable as soon as they are created since this
    // function sets up the EIP.
    kproc->state_flags = PROC_RUNNABLE;

    return kproc;
}

// Used by assembly code to access the `registers` field of a struct proc.
uint32_t const REG_SAVE_OFFSET = offsetof(struct proc, registers);
uint32_t const KSTACK_BOTTOM_OFFSET = offsetof(struct proc, kernel_stack) +
    offsetof(struct stack, bottom);

// Perform the actual context switch from prev to next.
// @param prev: The process that was running up until this call. If this is the
// very first context switch on the current core then prev must be NULL.
// @param next: The process to branch the execution to.
// Note: This function will not change the address space, nor the curr_proc
// percpu variable. This MUST be done by the caller, before calling this
// function.
extern void do_context_switch(struct proc * prev, struct proc * next);

void switch_to_proc(struct proc * const proc) {
    struct proc * const prev = this_cpu_var(curr_proc);

    proc->cpu = cpu_id();

    // Switch to the next process' address space and update this cpu's curr_proc
    // variable. This won't be done by do_context_switch()!
    this_cpu_var(curr_proc) = proc;
    switch_to_addr_space(proc->addr_space);

    // Change the kernel stack of the TSS of this cpu. This is only required for
    // user processes since kernel processes will never have to switch privilege
    // level.
    if (!proc->is_kernel_proc) {
        change_tss_esp0(proc->kernel_stack.bottom);
    }

    // Perform the actual context switch.
    do_context_switch(prev, proc);
}

// Close all the files opened by a process.
// @param proc: The process for which all opened files should be closed.
static void close_all_opened_files(struct proc * const proc) {
    for (fd_t fd = 0; fd < MAX_FDS; ++fd) {
        struct file_table_entry * const entry = proc->file_table[fd];
        if (!entry) {
            continue;
        }
        vfs_close(entry->file);
        kfree(entry);
        // Reset the entry to avoid use after free.
        proc->file_table[fd] = NULL;
    }
}

void delete_proc(struct proc * const proc) {
    // Close any file that remained opened until now.
    close_all_opened_files(proc);

    dealloc_stack(&proc->kernel_stack);
    if (!proc->is_kernel_proc) {
        // Delete the address space for user processes, this will also
        // de-allocate the user stack.
        delete_addr_space(proc->addr_space);
    }
    kfree(proc);
}

#include <proc.test>
