// Some useful macros for assembly files

// Define a function in an assembly file. Make sure that the function is in the
// .text section and in the .global scope.
#define ASM_FUNC_DEF(name) ;\
    .section .text ;\
    .global name ;\
    .type name, @function ;\
    name

