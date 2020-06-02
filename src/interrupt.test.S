#include <macro.h>

ASM_FUNC_DEF(basic_interrupt_test_int):
    int     $0x0
    ret

.extern basic_interrupt_test_target
ASM_FUNC_DEF(basic_interrupt_test_handler):
    movb     $0xAA, basic_interrupt_test_target
    iret

.extern interrupt_register_save_expected 
ASM_FUNC_DEF(interrupt_register_save_do_int):
    push    %ebx
    push    %esi
    push    %edi

    mov     $0xAAAAAAAA, %eax
    mov     $0xBBBBBBBB, %ebx
    mov     $0xCCCCCCCC, %ecx
    mov     $0xDDDDDDDD, %edx
    mov     $0x55555555, %esi
    mov     $0x11111111, %edi

    // Set the expected struct register_save_area
    // interrupt_register_save_expected.
    mov     $interrupt_register_save_expected, %eax
    movl    %edi, (%eax)
    movl    %esi, 0x4(%eax)
    movl    %ebp, 0x8(%eax) 
    movl    %ebx, 0x10(%eax) 
    movl    %edx, 0x14(%eax) 
    movl    %ecx, 0x18(%eax) 
    movl    $0xAAAAAAAA, 0x1C(%eax) 
    pushf
    movl    %esp, 0xC(%eax) 
    movl    (%esp), %ebx
    movl    %ebx, 0x20(%eax)
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
    
    // Reset EAX and EBX were modified while setting the
    // interrupt_register_save_expected. Reset them to their expected value.
    mov     $0xAAAAAAAA, %eax
    mov     $0xBBBBBBBB, %ebx

    int     $0
exp_eip:
    // Removed saved eflags from stack.
    add     $0x4, %esp
    pop     %edi
    pop     %esi
    pop     %ebx
    ret
