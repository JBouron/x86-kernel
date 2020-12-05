#include <macro.h>
.intel_syntax   noprefix

//reg_t do_syscall_dispatch(void const * const func,
//                          struct syscall_args const * const args);
ASM_FUNC_DEF(do_syscall_dispatch):
    push    ebp
    mov     ebp, esp

    // EAX = function ptr.
    mov     eax, [esp + 0x8]
    // ECX = pointer on the struct syscall_args.
    mov     ecx, [esp + 0xC]
    // Push the arguments for the syscall onto the stack.
    push    [ecx + 0x18]
    push    [ecx + 0x14]
    push    [ecx + 0x10]
    push    [ecx + 0x0C]
    push    [ecx + 0x08]
    push    [ecx + 0x04]

    // Do the syscall.
    call    eax

    add     esp, 0x18
    pop     ebp
    ret
