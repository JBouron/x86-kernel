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

    // Pusha and Popa do not touch the EIP and EFLAGS. We need to save them
    // manually.
    reg_t eip;
    reg_t eflags;
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
// The register_save_area is zeroed (except for ESP and EIP).
// @param eip: The initial value of the EIP register for the new process. 
// @return: A pointer on the allocated struct proc.
struct proc *create_proc(void const * const eip);

// Switch the execution of the current cpu to a process. This function does not
// return.
// @param proc: The process to execute.
void switch_to_proc(struct proc * const proc);

// Execute tests related to proceses managment.
void proc_test(void);
