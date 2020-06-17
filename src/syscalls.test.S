#include <macro.h>

//void simple_syscall_test_code(void * unused);
ASM_FUNC_DEF(simple_syscall_test_code):
.global simple_syscall_test_code_start
simple_syscall_test_code_start:
    rdtsc
    mov     %eax, %ebx

    rdtsc
    mov     %eax, %ecx

    rdtsc
    mov     %eax, %edx

    rdtsc
    mov     %eax, %esi

    rdtsc
    mov     %eax, %edi

    rdtsc
    mov     %eax, %ebp

    mov     $0x0, %eax 
    int     $0x80

    mov     %eax, %ebx
    mov     $0x0, %eax
    int     $0x80
dead:
    jmp     dead
.global simple_syscall_test_code_end
simple_syscall_test_code_end:

//void open_syscall_test_code(void * unused);
ASM_FUNC_DEF(open_syscall_test_code):
.global open_syscall_test_code_start
open_syscall_test_code_start:
    // Skip strings and helper functions.
    jmp     Lstart
Lfirst:
.asciz "/open_syscall_test/root/file0"
Lsecond:
.asciz "/open_syscall_test/root/dir1/file1"

// Some helper nested functions.
// Call the open() syscall with a given path. The path is passed to Lcall_open
// on the stack like a regular function.
// This function returns the file descriptor.
Lcall_open:
    mov     0x4(%esp), %ebx
    mov     $0x2, %eax
    int     $0x80
    // EAX already contains the file descriptor.
    ret

Lstart:
    // Open first file.
    mov     $Lfirst, %eax
    push    %eax
    call    Lcall_open

    // Open second file.
    mov     $Lsecond, %eax
    push    %eax
    call    Lcall_open

Ldead:
    jmp     Ldead


.global open_syscall_test_code_end
open_syscall_test_code_end:

//void read_syscall_test_code(void * unused);
ASM_FUNC_DEF(read_syscall_test_code):
.global read_syscall_test_code_start
read_syscall_test_code_start:
    // Skip strings buffers and helper functions.
    jmp     rstc_start
rstc_filename:
.asciz "/read_syscall_test/root/file0"

.set BUF_SIZE, 16

// Some helper nested functions.
// Call the open() syscall with a given path. The path is passed to Lcall_open
// on the stack like a regular function.
// This function returns the file descriptor.
rstc_call_open:
    mov     0x4(%esp), %ebx
    mov     $0x2, %eax
    int     $0x80
    // EAX already contains the file descriptor.
    ret

rstc_call_read:
    mov     0x4(%esp), %ebx
    mov     0x8(%esp), %ecx
    mov     0xC(%esp), %edx
    mov     $0x3, %eax
    int     $0x80
    ret

rstc_start:
    // Open file.
    mov     $rstc_filename, %eax
    push    %eax
    call    rstc_call_open
    // EAX = fd

    // Make space on the stack to read into.
    sub     $BUF_SIZE, %esp
    // ECX = pointer to buffer.
    mov     %esp, %ecx

    // Read from the file until we cannot read anymore.
    pushl   $BUF_SIZE
    push    %ecx
    push    %eax
rstc_read_loop:
    call    rstc_call_read
    test    %eax, %eax
    jnz     rstc_read_loop

rstc_dead:
    jmp     rstc_dead


.global read_syscall_test_code_end
read_syscall_test_code_end:

//void getpid_syscall_test_code(void * unused);
ASM_FUNC_DEF(getpid_syscall_test_code):
.global getpid_syscall_test_code_start
getpid_syscall_test_code_start:
    mov     $0x4, %eax
    int     $0x80
gpst_dead:
    jmp     gpst_dead
.global getpid_syscall_test_code_end
getpid_syscall_test_code_end:
