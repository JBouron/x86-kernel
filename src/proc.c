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

// Force the execution of a process. This function will restore the registers
// and iret into the process code. This function is meant to be used for the
// very first time a process is running ONLY. Using this function directly will
// break the scheduler state.
// @param proc: The process to return into.
extern void initial_ret_to_proc(struct proc * const proc);

// This function is meant to be called if a kernel stack underflow occurs while
// context switching. This is a protection mechanism used when a function passed
// to init_kernel_stack returns.
static void catch_kstack_underflow(void) {
    PANIC("Kernel stack underflow");
}

// Initialize the proc's kernel stack so that the first context switch to this
// proc will call a particular function.
// @param proc: The proc for which the kernel stack should be initialzed. Its
// kernel stack should already been allocated.
// @param func: The function to be called by the proc while in kernel during the
// first context switch. This function must not return, otherwise a stack
// underflow will occur.
// @parm arg: The argument to the function.
static void init_kernel_stack(struct proc * const proc,
                              void (*func)(void*),
                              void * const arg) {
    // We need to manipulate the kernel stack of the proc in such a way that the
    // first call to _schedule() that will branch execution to this proc will
    // call a function (func) once it returns from _schedule().
    // Hence the initial kernel stack should look like the following (offsets
    // are relative to %ESP):
    // %ESP + 0x10     Arg for `func`, i.e. `arg`.
    // %ESP + 0x0C     Presumably caller of `func`. catch_kstack_underflow.
    // %ESP + 0x08     Second arg for _schedule() (next). More info below.
    // %ESP + 0x04     First arg for _schedule() (prev). More info below.
    // %ESP + 0x00     Return address for _schedule(), i.e. address of `func`.
    //
    // When a call to _schedule() will start executing the proc, the proc will
    // restore its kernel registers (default to 0) and ret from _schedule().
    // Since we crafted the return address to point to `func`, the proc will
    // start executing the `func` function. The return address for this call is
    // a dummy value, hence why `func` must never return. The argument for
    // `func` is found at the very bottom of the stack.
    //
    // Note: Because `func` is most likely not even aware of the fake
    // _schedule() call, it will not pop out the arguments of _schedule(). Hence
    // we need to pop them out within the _schedule() funtion hence the stdcall
    // attribute for _schedule().
    //
    // Note2: The arguments to _schedule() must be: prev == proc, next == dummy.
    // This is because we want to fake a context switch to the proc and hence
    // this is done by making the proc believe it executed a _schedule() in the
    // past. Since when executing _schedule() prev must point to the current
    // process, here it needs to point to `proc`. The `next` pointer does not
    // matter since it won't be used (only the second half of _schedule will be
    // executed).
    ASSERT(proc->kernel_stack.bottom);
    uint32_t * esp0 = proc->kernel_stack.bottom;

    // Push the arg for the function onto the stack.
    *esp0 = (uint32_t)arg;
    esp0 --;

    // Push a dummy address for the caller.
    *esp0 = (uint32_t)(void*)catch_kstack_underflow;
    esp0 --;

    // The second arg of _schedule().
    *esp0 = 0;
    esp0 --;

    // The first arg of _schedule().
    *esp0 = (uint32_t)(void*)proc;
    esp0 --;

    // Push the presumably caller of _schedule() onto the stack. That is the
    // function we are trying to execute.
    *esp0 = (uint32_t)(void*)func;

    // Set the saved esp to point to the last word on the stack.
    proc->kernel_registers.esp = (reg_t)esp0;
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
    struct proc * const proc =  create_proc_in_ring(3);
    if (!proc) {
        return NULL;
    }

    // Make the proc execute initial_ret_to_proc() upon the first execution.
    // This will create an interrupt stack frame onto the kernel stack and iret
    // from it straight to user space.
    init_kernel_stack(proc, (void*)initial_ret_to_proc, proc);
    return proc;
}

struct proc *create_kproc(void (*func)(void*), void * const arg) {
    struct proc * const kproc = create_proc_in_ring(0);
    if (!kproc) {
        return NULL;
    }

    init_kernel_stack(kproc, func, arg);

    // Kernel processes are runnable as soon as they are created since this
    // function sets up the EIP.
    kproc->state_flags = PROC_RUNNABLE;

    return kproc;
}

// The following constants are used by initial_ret_to_proc() to access various
// fields of the struct register_save_area.
// This value is used by proc_asm.S to know the offset of registers_save within
// the struct proc.
uint32_t const REGISTERS_SAVE_OFFSET = offsetof(struct proc, registers_save);
uint32_t const KERNEL_REG_SAVE_OFFSET = offsetof(struct proc, kernel_registers);
uint32_t const IS_KPROC_OFFSET = offsetof(struct proc, is_kernel_proc);
uint32_t const KERNEL_STACK_BOTTOM_OFFSET =
    offsetof(struct proc, kernel_stack) + offsetof(struct stack, bottom);

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

// Perform the actual context switch from prev to next.
// @param prev: The process that was running up until the call to _schedule.  If
// this is the very first context switch on the current core then prev must be
// NULL.
// @param next: The process to branch the execution to.
// Note: This function will not change the address space, nor the curr_proc
// percpu variable. This MUST be done by the caller, before calling this
// function.
extern __attribute__((stdcall))
    void _schedule(struct proc * const prev, struct proc * const next);

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

    struct proc * const prev = this_cpu_var(curr_proc);

    // Switch to the next process' address space and update this cpu's curr_proc
    // variable.
    this_cpu_var(curr_proc) = proc;
    switch_to_addr_space(proc->addr_space);

    // Perform the actual context switch.
    _schedule(prev, proc);
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
