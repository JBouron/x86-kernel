#pragma once
// Some useful macros for assembly files

// Define a function in an assembly file. Make sure that the function is in the
// .text section and in the .global scope.
#define ASM_FUNC_DEF(name) ;\
    .code32 ;\
    .section .text ;\
    .global name ;\
    .type name, @function ;\
    name

#define ASM_FUNC_DEF_16BITS(name) ;\
    .code16 ;\
    .section .text ;\
    .global name ;\
    .type name, @function ;\
    name

// Call a routine in the higher half from protected mode.
// This function must use an absolute call as the caller is in protected mode
// and therefore not in the higer half kernel.
// EAX is used for the absolute call and therefore is not saved.
#define CALL_FROM_PROTECTED_MODE(func_name) ;\
    mov     $ ## func_name, %eax ;\
    sub     $0xC0000000, %eax ;\
    call    *%eax
