#include <proc.h>
#include <memory.h>
#include <paging.h>
#include <frame_alloc.h>

// Helper function used to copy some code to the process' address space and set
// the EIP accodingly.
// @param proc: The process to copy the code to.
// @param code: Pointer on the code to be copied.
// @param len: The size of the code to be copied in bytes.
static inline void copy_code_to_proc(struct proc * const proc,
                                     void const * const code,
                                     size_t const len) {
    // For now this function does not support copying code that is more than 4KB
    // in size. This is ok as most of the test codes are very small anyways.
    ASSERT(len < PAGE_SIZE);

    // Allocate a new physical frame that will contain the code.
    void * code_frame = alloc_frame();
    // Map the frame to the current address space an copy the code.
    uint8_t * const mc = paging_map_frames_above(0x0, &code_frame, 1, VM_WRITE);
    memcpy(mc, code, len);
    paging_unmap(mc, PAGE_SIZE);

    // Now map the frame to the process' address space.
    uint32_t const flags = VM_USER | VM_NON_GLOBAL;
    void * const eip = paging_map_frames_above_in(proc->addr_space,
                                                  0x0,
                                                  &code_frame,
                                                  1,
                                                  flags);
    // Set the EIP to point to the copied code.
    proc->registers_save.eip = (reg_t)eip;
}

// Helper function to execute a process. This function is meant to be used by a
// remote call in order to start the execution of the process on a remote cpu.
// This is by no mean a scheduler and the remote cpu must be restarted after
// that since it will be stuck executing the process forever.
// @param proc: Pointer on a struct proc to execute.
static inline void exec_proc(void * proc) {
    // We need to reset the curr_proc to NULL, otherwise, _schedule() will
    // wrongly think the current stack is the kernel stack of a process (which
    // does not exist anymore).
    this_cpu_var(curr_proc) = NULL;

    switch_to_proc(proc);

    // Since proc is the only proc capable of running on the current cpu, the
    // switch_to_proc() above will not return.
    __UNREACHABLE__;
}
