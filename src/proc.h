#pragma once
#include <addr_space.h>
#include <list.h>
#include <percpu.h>
#include <fs.h>
#include <syscalls.h>

// Process related functions and types.

// This struct represent a snapshot of the general purpose registers of a
// process.
struct register_save_area {
    // The general purpose registers are listed in the order in which they are
    // pushed/poped from the stack using the pusha and popa instruction. This is
    // to make saving and restoring easier.
    reg_t edi;
    reg_t esi;
    reg_t ebp;
    reg_t esp;
    reg_t ebx;
    reg_t edx;
    reg_t ecx;
    reg_t eax;

    // The following registers are not pushed/poped by pusha/popa. The order
    // does not matter.
    reg_t eflags;
    reg_t eip;
    // For segment registers, the upper 16 bits should be ignored. They are
    // stored as 32 bits dwords because this is how they are pushed on the stack
    // by interrupts handlers.
    reg_t gs;
    reg_t fs;
    reg_t ds;
    reg_t ss;
    reg_t cs;
    reg_t es;
} __attribute__((packed));

// This struct contains the state of an opened file of a process.
struct file_table_entry {
    // The file in question, as returned by vfs_open().
    struct file * file;
    // Pointer within the file, that is the offset at which the next call to
    // read() write() will read from/write to.
    off_t file_pointer;
};

// The maximum number of opened file per process.
#define MAX_FDS 32

// Describe a process' stack.
struct stack {
    // The top of the stack, that is the lowest address pointing to a byte in
    // the stack.
    void * top;
    // The bottom of the stack, that is the highest address pointing to a byte
    // in the stack.
    void * bottom;
    // The size of the stack in number of pages.
    uint32_t num_pages;
};

// A process running on the system.
struct proc {
    // The private address space of this process.
    struct addr_space * addr_space;

    // The state of the registers at the time of the last context switch that
    // paused the execution of this process.
    struct register_save_area registers;

    // The stack used by the process while operating in user mode. Note that
    // kernel proceses do not use a user stack.
    struct stack user_stack;
    // The stack used by the process while operating in kernel mode. For kernel
    // stack, this is the only stack.
    struct stack kernel_stack;

    // This bit indicates if the process is a kernel process.
    bool is_kernel_proc;

    // The list_node used to enqueue processes in runqueues.
    struct list_node rq;

    // The cpu the process is currently enqueued in.
    uint8_t cpu;

    // The state of the process. A value of 0 indicate that this process is
    // runnable. See values below.
    uint32_t state_flags;

    // The exit code of the process. This field is only valid if the process is
    // dead.
    uint8_t exit_code;

    // The Process ID of this process.
    pid_t pid;

    // The file table contains the state of all the files opened by this
    // process. It maps a file descriptor to its corresponding file.
    struct file_table_entry *file_table[MAX_FDS];

    // The following is used for debugging purposes exclusively. It makes
    // possible to call hooks before and after calling a particular syscall.
#define _DEBUG_ALL_SYSCALLS -2UL
    // The syscall number for which the hooks should be called.
    uint32_t _debug_syscall_nr;
    // The function to call _before_ executing the syscall.
    void (*_pre_syscall_hook)(struct proc * proc,
                              struct syscall_args const * args);
    // The function to call _after_ executing the syscall.
    void (*_post_syscall_hook)(struct proc * proc,
                               struct syscall_args const * args,
                               reg_t res);

};

// Below are the flags used in the struct proc' state_flags field.
#define PROC_RUNNABLE       0x0 // Process can be enqueued in sched and run.
#define PROC_WAITING_EIP    0x1 // Process has uninitialized EIP.
#define PROC_DEAD           0x2 // Process is dead.

// Check if a process is runnable.
// @param p: A pointer on a struct proc.
// @return: true if the process is currently runnable, false otherwise.
#define proc_is_runnable(p) ((p)->state_flags == PROC_RUNNABLE)

// Check if a process is dead.
// @param p: A pointer on a struct proc.
// @return: true if the process is currently dead, false otherwise.
#define proc_is_dead(p)     ((p)->state_flags & PROC_DEAD)

// Create a new struct proc. The process' address space and stack are allocated.
// The register_save_area is zeroed, ESP points to the freshly allocated stack.
// @return: On success, a pointer on the allocated struct proc, NULL otherwise.
struct proc *create_proc(void);

// Kernel processes
// ================
//     Kernel processes are special processes that execute in ring 0. This means
// that they can access kernel data/code and even percpu data, unlike regular
// processes.
// Kernel processes do not have their own address space, instead they all share
// the kernel address space. They do, however, have their private stack.
// Kernel processes are created from a function pointer and an argument. The
// function acts as the code of the process.

// Create a kernel process asyncronously executing a kernel function.
// @param func: The function to execute in the kernel thread.
// @parma arg: The void* argument to pass to the function to be executed.
// @return: On success, a pointer on the initialized struct proc, otherwise NULL
// is returned.
struct proc *create_kproc(void (*func)(void*), void * const arg);

// Perform a context switch to a new process. This function will return when the
// execution of the current process resumes.
// @param proc: The process to execute.
void switch_to_proc(struct proc * const proc);

// Delete a process and all the physical frames mapped to its user address
// space.
// @param proc: The process to delete.
// Note: This function assumes that proc has been dynamically allocated.
void delete_proc(struct proc * const proc);

// Execute tests related to proceses managment.
void proc_test(void);
