#include <macro.h>
.intel_syntax   noprefix

ASM_FUNC_DEF(privileged_instr_in_ring3_code):
.global privileged_instr_in_ring3_code_start
privileged_instr_in_ring3_code_start:
    // CLI is a privileged instruction, this should generate a #GP.
    cli
.global privileged_instr_in_ring3_code_end
privileged_instr_in_ring3_code_end:

ASM_FUNC_DEF(register_saving_ring3_int_code):
.global register_saving_ring3_int_code_start
register_saving_ring3_int_code_start:
    // Make space on the stack to contain the expected register values twice:
    //  - Once for the values before the interrupt.
    //  - Second one for the values after the interrupt.
    mov     ecx, 64
    shl     ecx, 1
    sub     esp, ecx

    // Zero out the allocated space.
    mov     edi, esp
    cld
    mov     al, 0
    rep     stosb

    rdtsc
    mov     ecx, eax

    rdtsc
    mov     esi, eax

    rdtsc
    mov     edi, eax

    rdtsc
    mov     edx, eax

    mov     eax, 0xAAAAAAAA
    mov     ebx, 0xBBBBBBBB

    mov     eax, esp
    add     eax, 64
    mov     [eax + 0x00], edi
    mov     [eax + 0x04], esi
    mov     [eax + 0x08], ebp
    mov     [eax + 0x10], ebx
    mov     [eax + 0x14], edx
    mov     [eax + 0x18], ecx
    mov     DWORD PTR [eax + 0x1C], 0xAAAAAAAA
    pushf
    mov     ebx, [esp]
    mov     [eax + 0x20], ebx
    mov     [eax + 0x0C], esp
    mov     DWORD PTR [eax + 0x24], OFFSET FLAT : exp_eip
    mov     ebx, gs
    mov     [eax + 0x28], ebx
    mov     ebx, fs
    mov     [eax + 0x2C], ebx
    mov     ebx, ds
    mov     [eax + 0x30], ebx
    mov     ebx, ss
    mov     [eax + 0x34], ebx
    mov     ebx, cs
    mov     [eax + 0x38], ebx
    mov     ebx, es
    mov     [eax + 0x3C], ebx
    
    // Reset EAX and EBX were modified while writting the values to the stack.
    // Reset them to their expected value.
    mov     eax, 0xAAAAAAAA
    mov     ebx, 0xBBBBBBBB

    // We can now raise a soft interrupt.
    int     0x80
exp_eip:
    // Store current values of registers onto the second area on the stack.
    mov     [esp + 0x20], eax
    mov     [esp + 0x14], ebx
    mov     [esp + 0x10], esp
    mov     eax, esp

    pushf
    mov     ebx, [esp]

    add     eax, 4

    mov     [eax + 0x20], ebx

    mov     [eax + 0x00], edi
    mov     [eax + 0x04], esi
    mov     [eax + 0x08], ebp
    mov     [eax + 0x14], edx
    mov     [eax + 0x18], ecx
    // Can't compare the EIP. Use placeholder value.
    mov     DWORD PTR [eax + 0x24], OFFSET FLAT : exp_eip
    mov     ebx, gs
    mov     [eax + 0x28], ebx
    mov     ebx, fs
    mov     [eax + 0x2C], ebx
    mov     ebx, ds
    mov     [eax + 0x30], ebx
    mov     ebx, ss
    mov     [eax + 0x34], ebx
    mov     ebx, cs
    mov     [eax + 0x38], ebx
    mov     ebx, es
    mov     [eax + 0x3C], ebx

    add     esp, 0x8

    // Compare the two snapshot of registers at (%esp) and 0x40(%esp).
    cld
    lea     esi, [esp]
    lea     edi, [esp + 0x40]
    mov     ecx, 0x40
    rep     cmpsb
    je      same
not_same:
    mov     eax, 0
    jmp     do_int
same:
    mov     eax, 1
do_int:
    int     0x80

.global register_saving_ring3_int_code_end
register_saving_ring3_int_code_end:

.extern kernel_access_from_ring3_kernel_var
ASM_FUNC_DEF(kernel_access_from_ring3_code):
.global kernel_access_from_ring3_code_start
kernel_access_from_ring3_code_start:

kernel_access_from_ring3_expected_offset:
    mov     DWORD PTR [kernel_access_from_ring3_kernel_var], 0xBEEFBABE

.global kernel_access_from_ring3_code_end
kernel_access_from_ring3_code_end:
