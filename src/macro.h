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
