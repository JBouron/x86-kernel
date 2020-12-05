#include <macro.h>
.intel_syntax   noprefix

//void simple_syscall_test_code(void * unused);
ASM_FUNC_DEF(simple_syscall_test_code):
.global simple_syscall_test_code_start
simple_syscall_test_code_start:
    rdtsc
    mov     ebx, eax

    rdtsc
    mov     ecx, eax

    rdtsc
    mov     edx, eax

    rdtsc
    mov     esi, eax

    rdtsc
    mov     edi, eax

    rdtsc
    mov     ebp, eax

    mov     eax, 0
    int     0x80

    mov     ebx, eax
    mov     eax, 0
    int     0x80
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
    mov     ebx, [esp + 0x4]
    mov     eax, 0x2
    int     0x80
    // EAX already contains the file descriptor.
    ret

Lstart:
    // Open first file.
    mov     eax, OFFSET FLAT : Lfirst
    push    eax
    call    Lcall_open

    // Open second file.
    mov     eax, OFFSET FLAT : Lsecond
    push    eax
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
    mov     ebx, [esp + 0x4]
    mov     eax, 0x2
    int     0x80
    // EAX already contains the file descriptor.
    ret

rstc_call_read:
    mov     ebx, [esp + 0x4]
    mov     ecx, [esp + 0x8]
    mov     edx, [esp + 0xC]
    mov     eax, 0x3
    int     0x80
    ret

rstc_start:
    // Open file.
    mov     eax, OFFSET FLAT : rstc_filename
    push    eax
    call    rstc_call_open
    // EAX = fd

    // Make space on the stack to read into.
    sub     esp, BUF_SIZE
    // ECX = pointer to buffer.
    mov     ecx, esp

    // Read from the file until we cannot read anymore.
    push    BUF_SIZE
    push    ecx
    push    eax
rstc_read_loop:
    call    rstc_call_read
    test    eax, eax
    jnz     rstc_read_loop

rstc_dead:
    jmp     rstc_dead


.global read_syscall_test_code_end
read_syscall_test_code_end:


//void write_syscall_test_code(void * unused);
ASM_FUNC_DEF(write_syscall_test_code):
.global write_syscall_test_code_start
write_syscall_test_code_start:
    // Skip strings buffers and helper functions.
    jmp     wstc_start
wstc_filename:
.asciz "/write_syscall_test/root/file0"
wstc_buffer:
.byte   0xAA
.byte   0xAB
.byte   0xAC
.byte   0xAD
.byte   0xAE
.byte   0xAF
.byte   0xBA
.byte   0xBB
.byte   0xBC
.byte   0xBD
.byte   0xBE
.byte   0xBF
.byte   0xCA
.byte   0xCB
.byte   0xCC
.byte   0xCD
.byte   0xCE
.byte   0xCF
wstc_buffer_end:

// Some helper nested functions.
// Call the open() syscall with a given path. The path is passed to Lcall_open
// on the stack like a regular function.
// This function returns the file descriptor.
wstc_call_open:
    mov     ebx, [esp + 0x4]
    mov     eax, 0x2
    int     0x80
    // EAX already contains the file descriptor.
    ret

wstc_call_write:
    mov     ebx, [esp + 0x4]
    mov     ecx, [esp + 0x8]
    mov     edx, [esp + 0xC]
    mov     eax, 0x5
    int     0x80
    ret

wstc_start:
    // Open file.
    mov     eax, OFFSET FLAT : wstc_filename
    push    eax
    call    wstc_call_open
    // EAX = fd
    // ECX = buffer size
    mov     ecx, OFFSET FLAT : wstc_buffer_end
    sub     ecx, OFFSET FLAT : wstc_buffer

    // Write to the file until we cannot write anymore.
    push    ecx
    push    OFFSET FLAT : wstc_buffer
    push    eax
wstc_write_loop:
    call    wstc_call_write
    test    eax, eax
    jnz     wstc_write_loop

wstc_dead:
    jmp     wstc_dead
.global write_syscall_test_code_end
write_syscall_test_code_end:


//void getpid_syscall_test_code(void * unused);
ASM_FUNC_DEF(getpid_syscall_test_code):
.global getpid_syscall_test_code_start
getpid_syscall_test_code_start:
    mov     eax, 0x4
    int     0x80
gpst_dead:
    jmp     gpst_dead
.global getpid_syscall_test_code_end
getpid_syscall_test_code_end:
