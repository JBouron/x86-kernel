#include <macro.h>
.intel_syntax   noprefix

ASM_FUNC_DEF(basic_interrupt_test_int):
    int     0x0
    ret

.extern basic_interrupt_test_target
ASM_FUNC_DEF(basic_interrupt_test_handler):
    movb     [basic_interrupt_test_target], 0xAA
    iret

.extern interrupt_register_save_expected 
ASM_FUNC_DEF(interrupt_register_save_do_int):
    push    ebx
    push    esi
    push    edi

    mov     eax, 0xAAAAAAAA
    mov     ebx, 0xBBBBBBBB
    mov     ecx, 0xCCCCCCCC
    mov     edx, 0xDDDDDDDD
    mov     esi, 0x55555555
    mov     edi, 0x11111111

    // Set the expected struct register_save_area
    // interrupt_register_save_expected.
    mov     eax, OFFSET FLAT : interrupt_register_save_expected
    mov     [eax], %edi
    mov     [eax + 0x04], esi
    mov     [eax + 0x08], ebp
    mov     [eax + 0x10], ebx
    mov     [eax + 0x14], edx
    mov     [eax + 0x18], ecx
    mov     DWORD PTR [eax + 0x1C], 0xAAAAAAAA
    pushf
    mov     [eax + 0xC], esp
    mov     ebx, [esp]
    mov     [eax + 0x20], ebx
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
    
    // Reset EAX and EBX were modified while setting the
    // interrupt_register_save_expected. Reset them to their expected value.
    mov     eax, 0xAAAAAAAA
    mov     ebx, 0xBBBBBBBB

    int     0
exp_eip:
    // Removed saved eflags from stack.
    add     esp, 0x4
    pop     edi
    pop     esi
    pop     ebx
    ret
