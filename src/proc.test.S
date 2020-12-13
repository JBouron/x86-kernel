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

.section .data
save_registers_test_routine_data:
// Size = (8 GP + EFLAGS) * 4 + ret address + arg. = 44
.skip   0x2C
.global save_registers_test_routine_data_end
save_registers_test_routine_data_end:

ASM_FUNC_DEF(save_registers_test_routine):
    // Set registers to some values and call the save_registers() function
    // to save them.
    push    ebp
    mov     ebp, esp
    pusha

    // For the segment registers, all we need to do is to set them to something
    // different than we they currently are. At this point we have:
    //  CS = Kernel code segment.
    //  DS = ES = FS = SS = kernel data segment.
    //  GS = percpu segment of current cpu.
    // CS, DS and SS cannot change, they must stay the same (or we would need
    // duplicate entries at different indices in the GDT).
    // For ES, FS and GS we can do:
    //  ES = FS = percpu segment.
    //  GS = kernel data segment.
    mov     ax, gs
    mov     cx, es
    mov     es, ax
    mov     fs, ax
    mov     gs, cx

    // For ESP we need to account for the fact that we need to call
    // save_registers and therefore we cannot set a random value.
    // save_registers_test_routine_data is big enough to call save_registers.
    mov     eax, esp
    lea     esp, [save_registers_test_routine_data_end]
    // Save the previous ESP.
    push    eax

    // Push EIP override to save_registers.
    push    0xDEADC0DE

    // Push the dest argument to save_registers. From now on the ESP does not
    // change.
    push    [ebp + 0x8]

    // Indicate to the test functions the expected values of ESP, EIP and
    // EFLAGS. This is because those cannot be randomly assigned and hard to
    // predict.
.extern save_registers_test_exp_esp
.extern save_registers_test_exp_eflags
    mov     [save_registers_test_exp_esp], esp
    pushf
    pop     [save_registers_test_exp_eflags]

    // For GP registers, just set anything.
    mov     eax, 0xAAAAAAAA
    mov     ebx, 0xBBBBBBBB
    mov     ecx, 0xCCCCCCCC
    mov     edx, 0xDDDDDDDD
    mov     esi, 0xDEADBEEF
    mov     edi, 0xBEEFBABE
    mov     ebp, 0xDEADBABE

    call    save_registers
0:
    add     esp, 8

    // Restore the original ESP
    pop     esp

    // Before leaving restore the segment registers.
    call    set_segment_registers_for_kernel

    // Restore the GP and return.
    popa
    leave
    ret 

//void switch_to_proc_test_routine(void)
ASM_FUNC_DEF(switch_to_proc_test_routine):
    // At this point the stack contains a struct register_save_area containing
    // the expected values of the registers.
    // Create a new struct register_save_area containing the current values of
    // the registers, onto the stack.

    // ESP points to a struct switch_to_proc_test_data. We need to fill its
    // struct register_save_area. This struct starts at ESP + 0x4 (skipping the
    // flag). We cannot use save_registers here as we are not sure to have a
    // correct DS.
    mov     [esp + 0x04], edi
    mov     [esp + 0x08], esi
    mov     [esp + 0x0C], ebp
    mov     [esp + 0x10], esp
    mov     [esp + 0x14], ebx
    mov     [esp + 0x18], edx
    mov     [esp + 0x1C], ecx
    mov     [esp + 0x20], eax

    pushf
    pop     eax
    mov     [esp + 0x24], eax

    call    0f
0:
    lea     eax, [0b] 
    lea     ebx, [switch_to_proc_test_routine]
    // EAX = Offset between switch_to_proc_test_routine and 0.
    sub     eax, ebx
    pop     ebx
    sub     ebx, eax
    mov     [esp + 0x28], ebx

    xor     eax, eax
    mov     ax, gs
    mov     [esp + 0x2C], ax
    mov     ax, fs
    mov     [esp + 0x30], ax
    mov     ax, ds
    mov     [esp + 0x34], ax
    mov     ax, ss
    mov     [esp + 0x38], ax
    mov     ax, cs
    mov     [esp + 0x3C], ax
    mov     ax, es
    mov     [esp + 0x40], ax

    // Set the flag.
    mov     DWORD PTR [esp], 1
1: 
    pause
    jmp     1b
    
    
.global switch_to_proc_test_routine_end
switch_to_proc_test_routine_end:
