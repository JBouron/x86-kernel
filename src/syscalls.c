#include <syscalls.h>
#include <percpu.h>
#include <debug.h>
#include <sched.h>
#include <interrupt.h>
#include <vfs.h>
#include <kmalloc.h>
#include <memory.h>
#include <string.h>

// The mapping syscall number -> function.
static void *SYSCALL_MAP[] = {
    // Syscall 0 is used for testing purposes.
    [NR_SYSCALL_TEST]     =   NULL,
    [NR_SYSCALL_EXIT]     =   (void*)do_exit,
    [NR_SYSCALL_OPEN]     =   (void*)do_open,
    [NR_SYSCALL_READ]     =   (void*)do_read,
    [NR_SYSCALL_GETPID]   =   (void*)do_get_pid,
    [NR_SYSCALL_WRITE]    =   (void*)do_write,
    [NR_SYSCALL_KLOG]     =   (void*)do_klog,
};

// The number of entries in the SYSCALL_MAP.
#define SYSCALL_MAP_SIZE    (sizeof(SYSCALL_MAP) / sizeof(*SYSCALL_MAP))

// Check if a syscall number is within the bounds of the SYSCALL_MAP.
// @param syscall_nr: The syscall number.
// @return: true if `syscall_nr` falls within the bounds of SYSCALL_MAP, false
// otherwise.
static bool in_syscall_map_bounds(uint32_t syscall_nr) {
    return syscall_nr < SYSCALL_MAP_SIZE;
}

// Do the actual dispatch in assembly in order to have a generic way to execute
// syscall no matter the number of arguments.
// @param func: The function implementing the syscall.
// @param args: The parameter of the syscall.
reg_t do_syscall_dispatch(void const * const func,
                          struct syscall_args const * const args);

// Dispatch a syscall given the values of the registers.
// @param args: The arguments to the syscall, including the syscall number (in
// EAX).
// @return: The return value of the syscall.
static reg_t syscall_dispatch(struct syscall_args const * const args) {
    uint32_t const syscall_nr = args->eax;

    if (!in_syscall_map_bounds(syscall_nr)) {
        PANIC("Invalid syscall number: %u\n", syscall_nr);
    }

    void const * const func = SYSCALL_MAP[syscall_nr];

    if (!func) {
        // Even if the syscall number is valid, the entry could still give a
        // NULL pointer (in the case of the test syscall for instance).
        PANIC("Unimplemented syscall number: %u\n", syscall_nr);
    }
    struct proc * const curr = this_cpu_var(curr_proc);
    uint32_t const debug_n = curr->_debug_syscall_nr;

    // Do we need to debug that syscall ?
    bool const debug = debug_n == syscall_nr || debug_n == _DEBUG_ALL_SYSCALLS;

    if (debug && curr->_pre_syscall_hook) {
        curr->_pre_syscall_hook(curr, args);
    }

    reg_t const res = do_syscall_dispatch(func, args);

    if (debug && curr->_post_syscall_hook) {
        curr->_post_syscall_hook(curr, args, res);
    }
    return res;
}

// The interrupt handler for syscalls.
// @param frame: The interrupt frame of this syscall.
static void syscall_int_handler(struct interrupt_frame const * const frame) {
    // Prepare the struct syscall_args.
    struct syscall_args args = {
        .eax = frame->registers->eax,
        .ebx = frame->registers->ebx,
        .ecx = frame->registers->ecx,
        .edx = frame->registers->edx,
        .esi = frame->registers->esi,
        .edi = frame->registers->edi,
        .ebp = frame->registers->ebp,
    };

    reg_t const res = syscall_dispatch(&args);

    // FIXME: Need to cast here since the frame is a const ptr.
    struct register_save_area * regs =
        (struct register_save_area*)frame->registers;
    regs->eax = res;
}

void syscall_init(void) {
    interrupt_register_global_callback(SYSCALL_VECTOR, syscall_int_handler);
}

// Used by tests to revert the syscall_init(). This should not be used anywhere
// else.
static void syscall_revert_init(void) {
    interrupt_delete_global_callback(SYSCALL_VECTOR);
}

// Below are the definitions of the syscalls.

void do_exit(uint8_t const exit_code) {
    // For now, do_exit is trivial, we only set the state of the process to
    // dead. The exit code is stored, but not used yet.
    struct proc * const curr = this_cpu_var(curr_proc);
    LOG("[%u] Process %p is dead\n", this_cpu_var(cpu_id), curr);
    curr->exit_code = exit_code;
    curr->state_flags |= PROC_DEAD;

    // Now that the process is dead, run a round of scheduling. This will switch
    // to a new process.
    sched_run_next_proc(NULL);
    __UNREACHABLE__;
}

fd_t do_open(pathname_t const u_path) {
    pathname_t const path = memdup(u_path, strlen(u_path) + 1);
    struct proc * const curr = this_cpu_var(curr_proc);

    // Open the file.
    struct file * const file = vfs_open(path);
    ASSERT(file);

    // vfs_open() is supposed to make a copy of path if it is going to use it.
    kfree((char*)path);

    // Allocate a new entry for the file table.
    struct file_table_entry * const op_file = kmalloc(sizeof(*op_file));
    op_file->file = file;
    op_file->file_pointer = 0x0;

    // Find the next available id in the process's file table.
    fd_t fd = 0;
    for (fd = 0; fd < MAX_FDS && curr->file_table[fd]; ++fd);
    if (fd == MAX_FDS) {
        PANIC("No FDs left for process %u.\n", curr->pid);
    }

    // Insert the entry in the file table.
    curr->file_table[fd] = op_file;
    return fd;
}

size_t do_read(fd_t const fd, uint8_t * const buf, size_t const len) {
    struct proc * const curr = this_cpu_var(curr_proc);

    // Get the file from the file table.
    struct file_table_entry * const op_file = curr->file_table[fd];
    if (!op_file) {
        PANIC("Invalid fd %u for process %u\n", fd, curr->pid);
    }

    size_t const ret = vfs_read(op_file->file, op_file->file_pointer, buf, len);

    op_file->file_pointer += ret;
    return ret;
}

size_t do_write(fd_t const fd, uint8_t const * const buf, size_t const len) {
    struct proc * const curr = this_cpu_var(curr_proc);

    // Get the file from the file table.
    struct file_table_entry * const op_file = curr->file_table[fd];
    if (!op_file) {
        PANIC("Invalid fd %u for process %u\n", fd, curr->pid);
    }

    size_t const ret = vfs_write(op_file->file,
                                 op_file->file_pointer,
                                 buf,
                                 len);

    op_file->file_pointer += ret;
    return ret;

}

pid_t do_get_pid(void) {
    struct proc * const curr = this_cpu_var(curr_proc);
    return curr->pid;
}

void do_klog(char const * const message) {
    struct proc * const curr = this_cpu_var(curr_proc);
    uint64_t const tsc = read_tsc();
    pid_t const pid = curr->pid;
    uint8_t const cpu = this_cpu_var(cpu_id);
    LOG("[%X | cpu %u | pid %u] %s", tsc, cpu, pid, message);
}

#include <syscalls.test>
