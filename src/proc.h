#pragma once
#include <addr_space.h>
#include <list.h>
#include <percpu.h>

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

// A process running on the system.
struct proc {
    // The private address space of this process.
    struct addr_space * addr_space;
    // When the process is not currently running on a cpu, its general purpose
    // registers are saved here.
    struct register_save_area registers_save;

    // Pointers on the top and bottom of the stack. Those addresses are virtual
    // addresses from the proc's address space.
    void * stack_top;
    void * stack_bottom;
} __attribute__((packed));

// Create a new struct proc. The process' address space and stack are allocated.
// The register_save_area is zeroed, ESP points to the freshly allocated stack.
// @return: A pointer on the allocated struct proc.
struct proc *create_proc(void);

// Switch the execution of the current cpu to a process. This function does not
// return.
// @param proc: The process to execute.
void switch_to_proc(struct proc * const proc);

// Delete a process and all the physical frames mapped to its user address
// space.
// @param proc: The process to delete.
// Note: This function assumes that proc has been dynamically allocated.
void delete_proc(struct proc * const proc);

// Execute tests related to proceses managment.
void proc_test(void);
