#include <macro.h>

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
    mov     $64, %ecx
    shl     $1, %ecx
    sub     %ecx, %esp

    // Zero out the allocated space.
    mov     %esp, %edi
    cld
    mov     $0x0, %al
    rep     stosb

    rdtsc
    mov     %eax, %ecx

    rdtsc
    mov     %eax, %esi

    rdtsc
    mov     %eax, %edi

    rdtsc
    mov     %eax, %edx

    mov     $0xAAAAAAAA, %eax
    mov     $0xBBBBBBBB, %ebx

    mov     %esp, %eax
    add     $64, %eax
    movl    %edi, (%eax)
    movl    %esi, 0x4(%eax)
    movl    %ebp, 0x8(%eax) 
    movl    %ebx, 0x10(%eax) 
    movl    %edx, 0x14(%eax) 
    movl    %ecx, 0x18(%eax) 
    movl    $0xAAAAAAAA, 0x1C(%eax) 
    pushf
    movl    (%esp), %ebx
    movl    %ebx, 0x20(%eax)
    movl    %esp, 0xC(%eax) 
    movl    $exp_eip, 0x24(%eax)
    movl    %gs, %ebx
    movl    %ebx, 0x28(%eax)
    movl    %fs, %ebx
    movl    %ebx, 0x2C(%eax)
    movl    %ds, %ebx
    movl    %ebx, 0x30(%eax)
    movl    %ss, %ebx
    movl    %ebx, 0x34(%eax)
    movl    %cs, %ebx
    movl    %ebx, 0x38(%eax)
    movl    %es, %ebx
    movl    %ebx, 0x3C(%eax)
    
    // Reset EAX and EBX were modified while writting the values to the stack.
    // Reset them to their expected value.
    mov     $0xAAAAAAAA, %eax
    mov     $0xBBBBBBBB, %ebx

    // We can now raise a soft interrupt.
    int     $0x80
exp_eip:
    // Store current values of registers onto the second area on the stack.
    movl    %eax, 0x20(%esp) 
    movl    %ebx, 0x14(%esp) 
    movl    %esp, 0x10(%esp) 
    mov     %esp, %eax

    pushf
    movl    (%esp), %ebx

    add     $4, %eax

    movl    %ebx, 0x20(%eax)

    movl    %edi, (%eax)
    movl    %esi, 0x4(%eax)
    movl    %ebp, 0x8(%eax) 
    movl    %edx, 0x14(%eax) 
    movl    %ecx, 0x18(%eax) 
    // Can't compare the EIP. Use placeholder value.
    movl    $exp_eip, 0x24(%eax)
    movl    %gs, %ebx
    movl    %ebx, 0x28(%eax)
    movl    %fs, %ebx
    movl    %ebx, 0x2C(%eax)
    movl    %ds, %ebx
    movl    %ebx, 0x30(%eax)
    movl    %ss, %ebx
    movl    %ebx, 0x34(%eax)
    movl    %cs, %ebx
    movl    %ebx, 0x38(%eax)
    movl    %es, %ebx
    movl    %ebx, 0x3C(%eax)

    add     $0x8, %esp

    // Compare the two snapshot of registers at (%esp) and 0x40(%esp).
    cld
    lea     (%esp), %esi
    lea     0x40(%esp), %edi
    mov     $0x40, %ecx
    rep     cmpsb
    je      same
not_same:
    mov     $0, %eax
    jmp     do_int
same:
    mov     $1, %eax
do_int:
    int     $0x80

.global register_saving_ring3_int_code_end
register_saving_ring3_int_code_end:

.extern kernel_access_from_ring3_kernel_var
ASM_FUNC_DEF(kernel_access_from_ring3_code):
.global kernel_access_from_ring3_code_start
kernel_access_from_ring3_code_start:

kernel_access_from_ring3_expected_offset:
    movl    $0xBEEFBABE, kernel_access_from_ring3_kernel_var

.global kernel_access_from_ring3_code_end
kernel_access_from_ring3_code_end:
