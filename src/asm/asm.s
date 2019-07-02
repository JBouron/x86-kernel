.section .text
.global read_msr
.type read_msr, @function
read_msr:
    push    %ebp
    mov     %esp, %ebp
    // Mov `msr_num` into %ecx
    mov     0x8(%ebp), %ecx
    rdmsr
    // *hi = %edx
    mov     0xC(%ebp), %ebx
    mov     %edx, (%ebx)
    // *low = %eax
    mov     0x10(%ebp), %ebx
    mov     %eax, (%ebx)
    pop     %ebp
    ret

.global read_tsc
.type read_tsc, @function
read_tsc:
    push    %ebp
    mov     %esp, %ebp
    rdtsc
    // *hi = %edx
    mov     0x8(%ebp), %ebx
    mov     %edx, (%ebx)
    // *low = %eax
    mov     0xC(%ebp), %ebx
    mov     %eax, (%ebx)
    pop     %ebp
    ret

.global interrupts_disable
.type interrupts_disable, @function
interrupts_disable:
    cli

.global interrupts_enable
.type interrupts_enable, @function
interrupts_enable:
    sti

.global load_gdt
.type load_gdt, @function
load_gdt:
    push    %ebp
    mov     %esp, %ebp
    mov     0x8(%ebp), %eax
    lgdt    (%eax)
    pop     %ebp
    ret

.global write_cs
.type write_cs, @function
write_cs:
    mov     0x4(%esp), %eax
# We must use a long jump to reload the value of $cs.
    ljmpl   $0x8, $end
end:
    ret

.global write_ds
.type write_ds, @function
write_ds:
    movw     0x4(%esp), %ds
    ret

.global write_ss
.type write_ss, @function
write_ss:
    movw     0x4(%esp), %ss
    ret

.global write_es
.type write_es, @function
write_es:
    movw     0x4(%esp), %es
    ret

.global write_fs
.type write_fs, @function
write_fs:
    movw     0x4(%esp), %fs
    ret

.global write_gs
.type write_gs, @function
write_gs:
    movw     0x4(%esp), %gs
    ret
