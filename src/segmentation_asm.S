#include <macro.h>
.intel_syntax   noprefix

ASM_FUNC_DEF(set_higher_half_segments):
    // Load the segment descriptors.
    mov     eax, [esp + 0x4]
    mov     cx, [esp + 0x8]
    mov     dx, [esp + 0xC]

    // Update the segment registers.
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     ss, cx
    mov     gs, dx

    // Fixup the ESP and EBP.
    add     esp, 0xC0000000
    add     ebp, 0xC0000000

    // We can now use the stack again. Prepare the far jump to the target in the
    // new code segment.
    mov     ecx, [esp + 0x10]
    push    eax
    push    ecx

    // Do the far jump
    jmp    FAR PTR [esp]
