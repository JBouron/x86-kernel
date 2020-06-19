#pragma once
#include <types.h>

// Syscalls.
// This files contains the syscall implementation.
// Syscalls are accessed through a software interrupt with vector SYSCALL_VECTOR
// (default 0x80). The kernel uses the values of the registers of the process
// raising the interrupt to perform the syscall as follows:
//  - EAX = Syscall number, that is the number associated to the function to be
//  performed.
//  - EBX, ECX, EDX, ESI, EDI, EBP = arguments to the syscall (in this order).
//
//  Once the syscall is performed, EAX contains the result of the syscall other
//  registers are untouched.

// The vector number used by syscalls.
#define SYSCALL_VECTOR  0x80

// Below is the list of syscall numbers.
#define NR_SYSCALL_TEST     0x0
#define NR_SYSCALL_EXIT     0x1
#define NR_SYSCALL_OPEN     0x2
#define NR_SYSCALL_READ     0x3
#define NR_SYSCALL_GETPID   0x4
#define NR_SYSCALL_WRITE    0x5
#define NR_SYSCALL_KLOG     0x6

// This struct represents the arguments passed to a syscall in order. This is
// the same order as in Linux for 32-bit kernels. However, the similarity ends
// here.
struct syscall_args {
    // EAX is the syscall number. EAX will also contain the return value of the
    // syscall.
    reg_t eax;
    // Below are the syscall params in order.
    reg_t ebx;
    reg_t ecx;
    reg_t edx;
    reg_t esi;
    reg_t edi;
    reg_t ebp;
};

// Initialize the syscall mechanism.
void syscall_init(void);

// Exit the current process. This function will obviously not return.
// @param exit_code: The value of the exit code.
void do_exit(uint8_t const exit_code);

// Open a file.
// @param path: The absolute path of the file to be opened.
// @return: A file descriptor of the opened file.
fd_t do_open(pathname_t const path);

// Read from a file descriptor.
// @param fd: The file descriptor to read from.
// @param buf: The buffer to read into.
// @param len: The size of the read/buffer.
// @return: The number of bytes read into buf.
size_t do_read(fd_t const fd, uint8_t * const buf, size_t const len);

// Write to a file descriptor.
// @param fd: The file descriptor to write into.
// @param buf: The data to write.
// @param len: The size of the data to be written, in bytes.
// @return: The number of bytes written.
size_t do_write(fd_t const fd, uint8_t const * const buf, size_t const len);

// Return the PID of the current process.
pid_t do_get_pid(void);

void do_klog(char const * const message);

// Execute syscall related tests.
void syscall_test(void);
