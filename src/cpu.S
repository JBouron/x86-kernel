#include <macro.h>
.intel_syntax   noprefix

// uint64_t read_msr(uint8_t const msr_num);
ASM_FUNC_DEF(read_msr):
    // rdmsr gets the MSR number from %ecx and returns the content in %edx:%eax.
    mov     ecx, [esp + 0x4] 
    rdmsr
    // Since %edx:%eax already contain the MSR value, no need to do anything.
    ret

// void write_msr(uint8_t const msr_num, uint64_t const val);
ASM_FUNC_DEF(write_msr):
    // Stack contains:
    //  |   val hi     | + 0xC
    //  |   val low    | + 0x8
    //  |   msr_num    | + 0x4
    //  |   ret addr   | <- esp
    mov     ecx, [esp + 0x4]
    mov     edx, [esp + 0xC]
    mov     eax, [esp + 0x8]
    // WRMSR writes %edx:%eax into MSR %ecx.
    wrmsr
    ret

ASM_FUNC_DEF(read_tsc):
    // RDTSC reads the TCS into %edx:%eax.
    rdtsc
    ret

ASM_FUNC_DEF(read_eflags):
    pushf
    pop     eax
    ret

ASM_FUNC_DEF(write_eflags):
    mov     eax, [esp + 0x4]
    push    eax
    popf
    ret

ASM_FUNC_DEF(lock_up):
0:
    cli
    hlt
    // In case we ever wake up from the halt, jump to the beginning and go back
    // to locked up mode.
    jmp     OFFSET FLAT : 0b

ASM_FUNC_DEF(has_cpuid):
    // From IA-32 manual:
    // "The ID flag (bit 21) in the EFLAGS register indicates support for the
    // CPUID instruction. If a software procedure can set and clear this flag,
    // the processor executing the procedure supports the CPUID instruction."
    pushf
    // Invert the value of the ID bit in EFLAGS.
    xor     DWORD PTR [esp], 0x200000
    // Save value of EFLAGS with inverted ID bit.
    mov     eax, [esp]
    // Reset EFLAGS with the new value having the inverted ID bit.
    popf
    // Re-read EFLAGS.
    pushf
    // Compare with the saved value in %eax, if CPUID is supported then the
    // inversion of the ID bit is successful and the value of EFLAGS just read
    // is the same as the saved value in %eax.
    cmp     eax, DWORD PTR [esp]
    jne     different
    // %eax and EFLAGS are the same, inversion was sucessful.
    mov     eax, 0x1
    jmp     end
different:
    // %eax and EFLAGS are different, inversion was not sucessful, return false.
    mov     eax, 0x0
end:
    // Discard value in top of stack.
    add     esp, 0x4
    ret 

// void cpuid_ecx(uint32_t i_eax, uint32_t i_ecx,
//                uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx,
//                uint32_t *o_edx);
ASM_FUNC_DEF(cpuid_ecx):
    push    ebp
    mov     ebp, esp
    
    // Write the value of EAX and ECX.
    mov     eax, [esp + 0x8]
    mov     ecx, [esp + 0xC]

    // Exectute CPUID. Save %ebx into the stack before hand.
    push    ebx 
    cpuid

    // Push the output of CPUID onto the stack.
    push    eax
    push    ebx
    push    ecx
    push    edx

    // For any non null destination write the value of the corresponding
    // register.
    // Make %eax point onto the last argument of the function ie. o_edx.
    lea     eax, [ebp + 28]
loop_start:
    mov     ecx, [eax]
    cmp     ecx, 0x0
    je      skip_null
    // The pointer is non null, store the value.
    mov     edx, [esp]
    mov     [ecx], edx
skip_null:
    // Remove output value from the stack.
    add     esp, 0x4
    // Go to next address in the argument list.
    sub     eax, 0x4
    lea     ecx, [ebp + 0xC]
    cmp     ecx, eax
    jne     loop_start

    // And we are done.
    // Restore EBX.
    pop     ebx
    pop     ebp
    ret

//void cpuid(uint32_t i_eax, uint32_t *o_eax, uint32_t *o_ebx, uint32_t *o_ecx,
//           uint32_t *o_edx);
ASM_FUNC_DEF(cpuid):
    // Since ECX is not provided in argument we need to use 0 and call
    // cpuid_ecx.
    // Make %eax point to the last argument, that is the higher argument in the
    // stack. There are 5 args in cpuid(), hence this is %esp + 4 * 5.
    lea     eax, [esp + 0x14]
    // Now push all the arguments necessary to call cpuid_eax(). Starting will
    // the four output addresses.
    push    [eax]
    push    [eax - 0x4]
    push    [eax - 0x8]
    push    [eax - 0xC]
    // The value to be used for ECX, 0 in this case since it is not given.
    push    0 
    // And the value of EAX.
    push    [eax - 0x10]
    // All arguments are pushed, we can call cpuid_eax.
    call    cpuid_ecx
    add     esp, 0x18
    ret

//void cpu_outb(uint16_t const port, uint8_t const byte);
ASM_FUNC_DEF(cpu_outb):
    // Output port must be in DX, while the value in AL.
    mov     dx, [esp + 0x4]
    mov     al, [esp + 0x8]
    out     dx, al
    ret

//void cpu_outw(uint16_t const port, uint16_t const byte);
ASM_FUNC_DEF(cpu_outw):
    // Output port must be in DX, while the value in AX.
    mov     dx, [esp + 0x4]
    mov     ax, [esp + 0x8]
    out     dx, ax
    ret

//uint8_t cpu_inb(uint16_t const port);
ASM_FUNC_DEF(cpu_inb):
    // Port addr must be in DX.
    mov     dx, [esp + 0x4]
    in      al, dx
    ret

//void cpu_lgdt(struct gdt_desc const * const table_desc);
ASM_FUNC_DEF(cpu_lgdt):
    mov     eax, [esp + 0x4]
    // The following is not really clear. LGDT should take a memory address as
    // operand, but we need to dereference this address (otherwise AS
    // complains).
    lgdt    [eax]
    ret

//void cpu_sgdt(struct gdt_desc * const dest);
ASM_FUNC_DEF(cpu_sgdt):
    mov     eax, [esp + 0x4]
    sgdt    [eax]
    ret

//void cpu_set_cs(union segment_selector_t const * const sel);
ASM_FUNC_DEF(cpu_set_cs):
    // We need to prepare a long jump. For this we need to push onto the stack
    // the code segment selector followed by the target address.
    // Let EAX be the code segment and ECX the target.

    // Read the value of the segment selector into EAX.
    mov     eax, [esp + 0x4]
    mov     ax, [eax]
    // We can already push EAX.
    push    eax

    push    OFFSET FLAT : target
    jmp     FAR PTR [esp]

target:
    add     esp, 0x8
    ret


// Macro for the cpu_set_{d, e, f, g, s}c().
#define set_segment_register(reg)   ;\
    mov     eax, [esp + 0x4]        ;\
    mov     ax, [eax]               ;\
    mov     reg, ax                 ;\
    ret

//void cpu_set_ds(union segment_selector_t const * const sel);
ASM_FUNC_DEF(cpu_set_ds):
    set_segment_register(%ds)

//void cpu_set_es(union segment_selector_t const * const sel);
ASM_FUNC_DEF(cpu_set_es):
    set_segment_register(%es)

//void cpu_set_fs(union segment_selector_t const * const sel);
ASM_FUNC_DEF(cpu_set_fs):
    set_segment_register(%fs)

//void cpu_set_gs(union segment_selector_t const * const sel);
ASM_FUNC_DEF(cpu_set_gs):
    set_segment_register(%gs)

//void cpu_set_ss(union segment_selector_t const * const sel);
ASM_FUNC_DEF(cpu_set_ss):
    set_segment_register(%ss)

#define read_segment_register(reg) ;\
    mov     eax, reg                  ;\
    ret

//uint16_t __cpu_read_cs(void);
ASM_FUNC_DEF(__cpu_read_cs):
    read_segment_register(%cs)

//uint16_t __cpu_read_ds(void);
ASM_FUNC_DEF(__cpu_read_ds):
    read_segment_register(%ds)

//uint16_t __cpu_read_es(void);
ASM_FUNC_DEF(__cpu_read_es):
    read_segment_register(%es)

//uint16_t __cpu_read_fs(void);
ASM_FUNC_DEF(__cpu_read_fs):
    read_segment_register(%fs)

//uint16_t __cpu_read_gs(void);
ASM_FUNC_DEF(__cpu_read_gs):
    read_segment_register(%gs)

//uint16_t __cpu_read_ss(void);
ASM_FUNC_DEF(__cpu_read_ss):
    read_segment_register(%ss)

//void cpu_set_interrupt_flag(bool const enable);
ASM_FUNC_DEF(cpu_set_interrupt_flag):
    mov     eax, [esp + 0x4]
    cmp     eax, 1
    jne     disable
    sti
    ret
disable:
    cli
    ret

//void cpu_lidt(struct idt_desc const * const idt_desc);
ASM_FUNC_DEF(cpu_lidt):
    mov     eax, [esp + 0x4]
    // The following is not really clear. LIDT should take a memory address as
    // operand, but we need to dereference this address (otherwise AS
    // complains).
    lidt    [eax]
    ret

//void cpu_sidt(struct idt_desc * const dest);
ASM_FUNC_DEF(cpu_sidt):
    mov     eax, [esp + 0x4]
    sidt    [eax]
    ret

// void cpu_mfence(void);
ASM_FUNC_DEF(cpu_mfence):
    mfence
    ret

//bool cpu_paging_enabled(void);
ASM_FUNC_DEF(cpu_paging_enabled):
    # PG bit (bit 31) of CR0 indicates whether or not paging is enabled.
    mov     eax, cr0
    shr     eax, 31
    ret

//void cpu_set_cr3(void const * const page_dir_addr);
ASM_FUNC_DEF(cpu_set_cr3):
    mov     eax, [esp + 0x4]
    mov     ecx, eax
    shl     ecx, 20
    jz      addr_is_aligned
    # The address passed as arg is not 4KiB aligned. Panic.
    # TODO: PANIC here.
    int     3
addr_is_aligned:
    # Load the 20 MSb of the addr in the 20 MSb of CR3.
    mov     ecx, 0x00000FFF
    mov     edx, cr3
    and     ecx, edx
    or      eax, ecx
    mov     cr3, eax
    ret

//uint32_t cpu_read_cr3(void);
ASM_FUNC_DEF(cpu_read_cr3):
    mov     eax, cr3
    ret

// Helper routine writting the correct bits in the correct control registers to
// activate paging. This routine does _not_ perform the absolute jump into the
// mapped kernel, this is left to the callee.
ASM_FUNC_DEF(cpu_enable_paging_bits):
    # Enable PG and WP bits.
    mov     eax, 0x80010000
    mov     ecx, cr0
    or      eax, ecx
    mov     cr0, eax
    ret

//void cpu_enable_paging(void);
ASM_FUNC_DEF(cpu_enable_paging):
    # Enable the paging bits.
    call    cpu_enable_paging_bits
    # Since we are already in higher half logical addresses thanks to the Boot
    # GDT, there is no need to do a far jump.
    ret

//void cpu_invalidate_tlb(void);
ASM_FUNC_DEF(cpu_invalidate_tlb):
    mov     eax, cr3
    mov     cr3, eax
    ret

//void *cpu_read_cr2(void);
ASM_FUNC_DEF(cpu_read_cr2):
    mov     eax, cr2
    ret
ASM_FUNC_DEF(cpu_read_cr0):
    mov     eax, cr0
    ret
ASM_FUNC_DEF(cpu_read_cr4):
    mov     eax, cr4
    ret

//void cpu_halt(void);
ASM_FUNC_DEF(cpu_halt):
    hlt
    ret

//void cpu_set_interrupt_flag_and_halt(void);
ASM_FUNC_DEF(cpu_set_interrupt_flag_and_halt):
    sti
    hlt
    ret

//void cpu_enable_cache(void);
ASM_FUNC_DEF(cpu_enable_cache):
    // Cache is enabled by clearing the CD (bit 30) and NW (bit 29) of the CR0
    // register.
    mov     eax, cr0
    and     eax, ~(1 << 29)
    and     eax, ~(1 << 30)
    mov     cr0, eax
    ret

//void cpu_pause(void);
ASM_FUNC_DEF(cpu_pause):
    pause
    ret

//void cpu_read_esp(void);
ASM_FUNC_DEF(cpu_read_esp):
    # This function returns the ESP at the time of the call, hence skip the
    # return value in the stack.
    lea     eax, [esp + 0x4]
    ret

//uint8_t cpu_apic_id(void);
ASM_FUNC_DEF(cpu_apic_id):
    push    ebx
    mov     eax, 1
    # Getting the APIC ID is perfomed by executing cpuid with EAX = 1. The
    # result is encoded in the bits 31:24 of EBX.
    cpuid
    shr     ebx, 24
    mov     eax, ebx
    pop     ebx
    ret


//bool interrupts_enabled(void);
ASM_FUNC_DEF(interrupts_enabled):
    pushf
    pop     eax
    and     eax, (1 << 9)
    shr     eax, 9
    ret

//void cpu_ltr(union segment_selector_t const segment_sel);
ASM_FUNC_DEF(cpu_ltr):
    mov     eax, [esp + 0x4]
    ltr     ax
    ret

ASM_FUNC_DEF(cpu_read_eip):
    mov     eax, [esp]
    ret
