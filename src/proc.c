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

    if (proc->is_kernel_proc || !kernel_stack) {
        // For kernel processes, the initial values of ESP and EBP are the
        // bottom of the kernel stack. For user processes this is the bottom of
        // the user stack.
        proc->registers_save.esp = (reg_t)stack_bottom;
        proc->registers_save.ebp = proc->registers_save.esp;
    }

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
    proc->is_kernel_proc = (ring == 0);

    // Kernel processes use the kernel address space.
    proc->addr_space =
        (ring == 0) ? get_kernel_addr_space() : create_new_addr_space();
    if (!proc->addr_space) {
        SET_ERROR("Cannot create address space for new process", ENONE);
        kfree(proc);
        return NULL;
    }

    // Zero out the register_save struct of the proc so that the first time it
    // gets run all registers will be 0 (except esp and ebp).
    memzero(&proc->registers_save, sizeof(proc->registers_save));

    if (!proc->is_kernel_proc) {
        // Allocate user stack for user processes.
        if (!allocate_stack(proc, false)) {
            SET_ERROR("Could not allocate user stack for process", ENONE);
            delete_addr_space(proc->addr_space);
            kfree(proc);
            return NULL;
        }
    }
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
    if (!kproc) {
        return NULL;
    }

    // Put the argument on the stack of the kproc. Note: stack_bottom points to
    // the first _valid_ dword on the stack. We can directly access the stack
    // here as the address space of the kproc is the kernel address space.
    ASSERT(get_curr_addr_space() == get_kernel_addr_space());
    ASSERT((void*)kproc->registers_save.esp == kproc->kernel_stack.bottom);
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

// Check that the saved segments registers for a process about to run on the
// current cpu are ok. This means: user segments for user processes and kernel
// segments (+ percpu segment) for kernel processes.
// @param proc: The process to check the segment registers for.
static void check_segments(struct proc const * const proc) {
    struct register_save_area const * const reg = &proc->registers_save;
    if (proc->is_kernel_proc) {
        uint16_t const kcode = kernel_code_selector().value;
        uint16_t const kdata = kernel_data_selector().value;
        ASSERT(reg->cs == kcode);
        ASSERT(reg->es == kdata);
        ASSERT(reg->ds == kdata);
        ASSERT(reg->fs == kdata);
        ASSERT(reg->gs == cpu_read_gs().value);
        ASSERT(reg->ss == kdata);
    } else {
        uint16_t const ucode = user_code_seg_sel().value;
        uint16_t const udata = user_data_seg_sel().value;
        ASSERT(reg->cs == ucode);
        ASSERT(reg->es == udata);
        ASSERT(reg->ds == udata);
        ASSERT(reg->fs == udata);
        ASSERT(reg->gs == udata);
        ASSERT(reg->ss == udata);
    }
}

void switch_to_proc(struct proc * const proc) {
    if (proc->is_kernel_proc) {
        // Kernel proc can use percpu variable. Since some of them might not be
        // tied to specific cpus we need to make sure that the GS segment of the
        // process correspond to the percpu area of the cpu it is running on. 
        // Note: We assume that the current GS on the cpu is the percpu area of
        // this cpu. This is ok since we are in the kernel.
        proc->registers_save.gs = cpu_read_gs().value;
    }

    // Check that the segments are sound.
    check_segments(proc);

    // Do the actual context switch.
    this_cpu_var(curr_proc) = proc;
    switch_to_addr_space(proc->addr_space);
    do_far_return_to_proc(proc);
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
