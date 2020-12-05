#include <macro.h>
.intel_syntax   noprefix

// This file contains all the interrupt handlers for all vectors from 0 to 255.
// Each individual interrupt handler is very small and only "jumps" to a common
// interrupt handling routine (interrupt_entry).
//
// interrupt_entry expects the stack to have the following:
//       :           _:__
//       |    SS      |  \__ Optional, only if there as been
//       |    ESP    _|__/   a privilege change due to the interrupt.
//       |  EFLAGS    |
//       |    CS      |
//       |    EIP    _|_
//       | Error Code_|_}--- For interrupt that do not generate a code, put 0.
// EIP ->|   Vector   |
//
// There are two things to perform in order to get a stack as described above
// and being able to jump to the interrupt_entry routine:
//
//  1 - Not all interrupts generate an error code, for those who do not, we need
//  to insert a 0 instead. The rationale is that we always want an error code
//  (real or 0) so that all interrupt are consistent before the iret (it is the
//  programmer's responsibility to remove the error before the iret).
//
//  2 - We need to figure out the vector of the interrupt and push it on the
//  stack.
//
// For #2 we use the following trick:
//  - Every interrupt handler (0-255) is a simple call to a function, that is a
//  near 32-bit `call` instruction, such an instruction is 5 bytes long.
//  - When this function is called the vector can be computed from the return
//  address stored onto the stack (by the call instruction in the handler) as
//  follows:
//          vector = (ret_addr - &interrupt_handler_0 - 5) / 5.
//          Note: The -5 comes from the fact that the return address will point
//          to the instruction after the call executed by the interrupt handler.
//  this assumes that all interrupt handlers are contiguous in memory and there
//  is no space in between them (in between the call instructions).
//
// Here are all the important piece of the interrupt handling logic:
// * interrupt_entry: Common entry point for all interrupts. Expects a precise
//   state on the stack (error code and vector).
// * _prepare_stack_for_interrupt_entry: This is the function that interrupt
//   handlers will call to. This function will figure out the vector of the
//   interrupt using the return address as described above and will jump to the
//   interrupt_entry routine.
// * _prepare_stack_for_interrupt_entry_no_error_code: Special function similar
//   to _prepare_stack_for_interrupt_entry but that insert a 0 for the error
//   code. This is the function that interrupt handler will call for interrupt
//   without error codes.

_prepare_stack_for_interrupt_entry_no_error_code:
    // We just entered an interrupt handler that called this routine. At this
    // point, the stack looks as follow (high addresses at the top):
    //  |       EFLAGS          |
    //  |         CS            |
    //  |        EIP            |
    //  |    Caller Ret Addr    | <- EIP
    // We need to insert a dummy error code (0) before preparing the stack for
    // the entry point.
    // Once the error code is pushed we need to trick
    // _prepare_stack_for_interrupt_entry as if it was called directly.

    // Push the return address to the top of the stack. This will trick
    // _prepare_stack_for_interrupt_entry.
    push    [%esp]

    // Overwrite the old return address with the dummy error code.
    mov     DWORD PTR [esp + 0x4], 0x0

    // No jump to _prepare_stack_for_interrupt_entry, just fall-through.
_prepare_stack_for_interrupt_entry:
    // We just entered an interrupt handler that called this routine. At this
    // point, the stack looks as follow (high addresses at the top):
    //  |    EFLAGS       |
    //  |      CS         |
    //  |      EIP        |
    //  |   Error Code    |
    //  | Caller Ret Addr | <- EIP

    // We need to replace the return address by the actual vector number.

    // Save all the registers used in the vector computation. Those are the
    // registers of the interrupted context!
    push    eax
    push    ebx
    push    edx
    
    // vector = (ret_addr - &interrupt_handler_0 - 5) / 5.
    mov     eax, [esp + 0xC]
    sub     eax, OFFSET FLAT : interrupt_handler_0
    mov     ebx, 0x5
    sub     eax, ebx
    cdq
    // EAX = vector number
    div     ebx

    // Overwrite the return address with the vector, we don't need it anymore.
    mov     [esp + 0xC], eax

    // Restore the registers.
    pop     edx
    pop     ebx
    pop     eax

    // No jump to interrupt_entry required, simply fall-through.

interrupt_entry:
    // The stack now looks as follows:
    //  |    EFLAGS      |
    //  |      CS        |
    //  |      EIP       |
    //  |   Error Code   |
    //  |     Vector     | <- EIP

    // Check if the vector has an associated error code. Note: We need to save
    // Save registers onto the stack in reverse order as they appear in struct
    // register_save_area. We cannot use general purpose registers (except ESP)
    // until the pusha.

    // Note: Depending on the processor, pushing the segment registers might not
    // zero the top 16 bits of the dword on the stack which means we can end up
    // with garbage values. To avoid that, precede all segment reg push with a
    // push 0; add 4, %esp so that we are sure the top 16-bits will always be
    // 0.
#define save_seg_reg(reg)   ;\
    push    0               ;\
    add     esp, 0x4        ;\
    push    reg

    // Save ES.
    save_seg_reg(es)

    // CS is already on the stack. Since we pushed %ES, the value on CS on the
    // stack is at offset 0x10 at this point.
    push    [esp + 0x10]

    // The current value of %SS might not be the original value of SS when the
    // processor got interrupted if a task in ring 3 was running. We will patch
    // it up later.
    save_seg_reg(ss)

    save_seg_reg(ds)
    save_seg_reg(fs)
    save_seg_reg(gs)

    // Push the EIP, it was pushed on the interrupt frame.
    push    [esp + 0x20]

    // Push EFLAGS, it was pushed on the interrupt frame as well.
    push    [esp + 0x2C]

    // Now push all general purpose registers at once.
    pusha

    // We are finally free to use any register. Set EBP to point to the stack
    // frame containing the original information pushed by the interrupt.
    mov     ebp, esp
    // 16 registers saved, one vector pushed, 6 elements in the original int
    // frame.
    add     ebp, 16 * 4 + 1 * 4 + 6 * 4 - 4
    // For future reference, the elements in the original interrupt frame and
    // the saved registers can be accessed using EBP as follows:
    // EBP ->|     SS     | (if applicable)
    // -0x04 |    ESP     | (if applicable)
    // -0x08 |   EFLAGS   |
    // -0x0C |     CS     |
    // -0x10 |    EIP     |
    // -0x14 | Error Code |
    // -0x18 |___Vector___|________ Below this line are saved registers ________
    // -0x1C |     ES     |
    // -0x20 |     CS     | (same as -0x0C(%ebp))
    // -0x24 |     SS     | (/!\ See fixup) 
    // -0x28 |     DS     |
    // -0x2C |     FS     |
    // -0x30 |     GS     |
    // -0x34 |    EIP     | (same as -0x10(%ebp)) 
    // -0x38 |   EFLAGS   | (same as -0x08(%ebp)) 
    // -0x3C |    EAX     |
    // -0x40 |    ECX     |
    // -0x44 |    EDX     |
    // -0x48 |    EBX     |
    // -0x4C |    ESP     | (/!\ See fixup)
    // -0x50 |    EBP     |
    // -0x54 |    ESI     |
    // -0x58 |    EDI     |


    // In case a privilege change happened, the saved values for the stack
    // segment register and ESP are wrong since those are the SS and ESP used by
    // the interrupt handler, _not_ the interrupted context.
    // If that is the case, then those values can be found at the top of the
    // handler's stack frame. We therefore need the fixup the registers in the
    // save area.

    // EAX = privilege level of current code segment.
    mov     eax, cs
    and     eax, 0x3
    // EBX = privilege level of interrupted context's code segment.
    mov     ebx, [ebp - 0xC]
    and     ebx, 0x3

    cmp     ebx, eax
    jne     do_fixup_priv_change

    // There was no privilege change then the saved SS is correct. However the
    // saved ESP is still incorrect as we pushed some values on the stack before
    // the pusha. We need to fix this.
    // Since there was no privilege change, the interrupt frame started at EBP
    // - 0x8 and therefore the value of ESP before the interrupt was EBP - 0x4
    // (pointing to the word before the EFLAGS).
    lea     eax, [ebp - 0x4]
    mov     [ebp - 0x4C], eax
    jmp     do_fixup_priv_change_end

do_fixup_priv_change:
    // Fixup the saved SS.
    mov     eax, [ebp + 0x00]
    mov     [ebp - 0x24], eax

    // Fixup the saved ESP.
    mov     eax, [ebp - 0x4]
    mov     [ebp - 0x4C], eax

    // Because the interrupted context was not the kernel, the current segment
    // registers (except CS and SS) are not kernel segment. In particular GS is
    // not set to the percpu segment. Fixup the segment register before
    // continuing.
    call    set_segment_registers_for_kernel

do_fixup_priv_change_end:
    // Construct the struct interrupt_frame_t for the generic interrupt handler,
    // onto the stack.

    // Pointer on saved registers.
    push    esp
    // Vector.
    push    [ebp - 0x18]
    // Error code.
    push    [ebp - 0x14]
    // EIP.
    push    [ebp - 0x10]
    // CS.
    push    [ebp - 0x0C]
    // EFLAGS.
    push    [ebp - 0x08]

    // Call the generic_interrupt_handler.
    push    esp
    call generic_interrupt_handler
    add     esp, 0x1C

    // The interrupt has been dealt with. We can now restore the registers and
    // return from the interrupt.

    // Restore DS, ES, FS, and GS. SS and CS are special cases and will be
    // handled separately.
    mov     ds, [ebp - 0x28]
    mov     es, [ebp - 0x1C]
    mov     fs, [ebp - 0x2C]
    mov     gs, [ebp - 0x30]

    // SS does not need to be restore because:
    // - If a priv change occured, the SS will be restored by the IRET.
    // - If no priv change occured, then the SS has to be the same as the
    // kernel's SS otherwise this interrupt handler would not even work.

    // Restore GP registers.
    popa
    // Skip the saved segment registers.
    add     esp, 0x20
    // Get rid of vector and error code.
    add     esp, 0x8
    
    // ESP points to interrupt frame, we're ready to go.
    iret


// Define one interrupt handler for every vector (0 through 255). Each handler
// is a call instruction to the following routine:
//      * _prepare_stack_for_interrupt_entry, for handler for which the
//      interrupt generates an error code.
//      * _prepare_stack_for_interrupt_entry_no_error_code, for handlers for
//      which the interrupt does not produce an error code and one should be
//      added before jumping to the interrupt_entry routine.

#define interrupt_handler_no_err_code(vector)                       ;\
    .global interrupt_handler_ ## vector                            ;\
    interrupt_handler_ ## vector :                                  ;\
        call    _prepare_stack_for_interrupt_entry_no_error_code
 
#define interrupt_handler_err_code(vector)          ;\
    .global interrupt_handler_ ## vector            ;\
    interrupt_handler_ ## vector :                  ;\
        call    _prepare_stack_for_interrupt_entry

interrupt_handler_no_err_code(0)
interrupt_handler_no_err_code(1)
interrupt_handler_no_err_code(2)
interrupt_handler_no_err_code(3)
interrupt_handler_no_err_code(4)
interrupt_handler_no_err_code(5)
interrupt_handler_no_err_code(6)
interrupt_handler_no_err_code(7)
interrupt_handler_err_code(8)
interrupt_handler_no_err_code(9)
interrupt_handler_err_code(10)
interrupt_handler_err_code(11)
interrupt_handler_err_code(12)
interrupt_handler_err_code(13)
interrupt_handler_err_code(14)
interrupt_handler_no_err_code(15)
interrupt_handler_no_err_code(16)
interrupt_handler_err_code(17)
interrupt_handler_no_err_code(18)
interrupt_handler_no_err_code(19)
interrupt_handler_no_err_code(20)
interrupt_handler_no_err_code(21)
interrupt_handler_no_err_code(22)
interrupt_handler_no_err_code(23)
interrupt_handler_no_err_code(24)
interrupt_handler_no_err_code(25)
interrupt_handler_no_err_code(26)
interrupt_handler_no_err_code(27)
interrupt_handler_no_err_code(28)
interrupt_handler_no_err_code(29)
interrupt_handler_no_err_code(30)
interrupt_handler_no_err_code(31)
interrupt_handler_no_err_code(32)
interrupt_handler_no_err_code(33)
interrupt_handler_no_err_code(34)
interrupt_handler_no_err_code(35)
interrupt_handler_no_err_code(36)
interrupt_handler_no_err_code(37)
interrupt_handler_no_err_code(38)
interrupt_handler_no_err_code(39)
interrupt_handler_no_err_code(40)
interrupt_handler_no_err_code(41)
interrupt_handler_no_err_code(42)
interrupt_handler_no_err_code(43)
interrupt_handler_no_err_code(44)
interrupt_handler_no_err_code(45)
interrupt_handler_no_err_code(46)
interrupt_handler_no_err_code(47)
interrupt_handler_no_err_code(48)
interrupt_handler_no_err_code(49)
interrupt_handler_no_err_code(50)
interrupt_handler_no_err_code(51)
interrupt_handler_no_err_code(52)
interrupt_handler_no_err_code(53)
interrupt_handler_no_err_code(54)
interrupt_handler_no_err_code(55)
interrupt_handler_no_err_code(56)
interrupt_handler_no_err_code(57)
interrupt_handler_no_err_code(58)
interrupt_handler_no_err_code(59)
interrupt_handler_no_err_code(60)
interrupt_handler_no_err_code(61)
interrupt_handler_no_err_code(62)
interrupt_handler_no_err_code(63)
interrupt_handler_no_err_code(64)
interrupt_handler_no_err_code(65)
interrupt_handler_no_err_code(66)
interrupt_handler_no_err_code(67)
interrupt_handler_no_err_code(68)
interrupt_handler_no_err_code(69)
interrupt_handler_no_err_code(70)
interrupt_handler_no_err_code(71)
interrupt_handler_no_err_code(72)
interrupt_handler_no_err_code(73)
interrupt_handler_no_err_code(74)
interrupt_handler_no_err_code(75)
interrupt_handler_no_err_code(76)
interrupt_handler_no_err_code(77)
interrupt_handler_no_err_code(78)
interrupt_handler_no_err_code(79)
interrupt_handler_no_err_code(80)
interrupt_handler_no_err_code(81)
interrupt_handler_no_err_code(82)
interrupt_handler_no_err_code(83)
interrupt_handler_no_err_code(84)
interrupt_handler_no_err_code(85)
interrupt_handler_no_err_code(86)
interrupt_handler_no_err_code(87)
interrupt_handler_no_err_code(88)
interrupt_handler_no_err_code(89)
interrupt_handler_no_err_code(90)
interrupt_handler_no_err_code(91)
interrupt_handler_no_err_code(92)
interrupt_handler_no_err_code(93)
interrupt_handler_no_err_code(94)
interrupt_handler_no_err_code(95)
interrupt_handler_no_err_code(96)
interrupt_handler_no_err_code(97)
interrupt_handler_no_err_code(98)
interrupt_handler_no_err_code(99)
interrupt_handler_no_err_code(100)
interrupt_handler_no_err_code(101)
interrupt_handler_no_err_code(102)
interrupt_handler_no_err_code(103)
interrupt_handler_no_err_code(104)
interrupt_handler_no_err_code(105)
interrupt_handler_no_err_code(106)
interrupt_handler_no_err_code(107)
interrupt_handler_no_err_code(108)
interrupt_handler_no_err_code(109)
interrupt_handler_no_err_code(110)
interrupt_handler_no_err_code(111)
interrupt_handler_no_err_code(112)
interrupt_handler_no_err_code(113)
interrupt_handler_no_err_code(114)
interrupt_handler_no_err_code(115)
interrupt_handler_no_err_code(116)
interrupt_handler_no_err_code(117)
interrupt_handler_no_err_code(118)
interrupt_handler_no_err_code(119)
interrupt_handler_no_err_code(120)
interrupt_handler_no_err_code(121)
interrupt_handler_no_err_code(122)
interrupt_handler_no_err_code(123)
interrupt_handler_no_err_code(124)
interrupt_handler_no_err_code(125)
interrupt_handler_no_err_code(126)
interrupt_handler_no_err_code(127)
interrupt_handler_no_err_code(128)
interrupt_handler_no_err_code(129)
interrupt_handler_no_err_code(130)
interrupt_handler_no_err_code(131)
interrupt_handler_no_err_code(132)
interrupt_handler_no_err_code(133)
interrupt_handler_no_err_code(134)
interrupt_handler_no_err_code(135)
interrupt_handler_no_err_code(136)
interrupt_handler_no_err_code(137)
interrupt_handler_no_err_code(138)
interrupt_handler_no_err_code(139)
interrupt_handler_no_err_code(140)
interrupt_handler_no_err_code(141)
interrupt_handler_no_err_code(142)
interrupt_handler_no_err_code(143)
interrupt_handler_no_err_code(144)
interrupt_handler_no_err_code(145)
interrupt_handler_no_err_code(146)
interrupt_handler_no_err_code(147)
interrupt_handler_no_err_code(148)
interrupt_handler_no_err_code(149)
interrupt_handler_no_err_code(150)
interrupt_handler_no_err_code(151)
interrupt_handler_no_err_code(152)
interrupt_handler_no_err_code(153)
interrupt_handler_no_err_code(154)
interrupt_handler_no_err_code(155)
interrupt_handler_no_err_code(156)
interrupt_handler_no_err_code(157)
interrupt_handler_no_err_code(158)
interrupt_handler_no_err_code(159)
interrupt_handler_no_err_code(160)
interrupt_handler_no_err_code(161)
interrupt_handler_no_err_code(162)
interrupt_handler_no_err_code(163)
interrupt_handler_no_err_code(164)
interrupt_handler_no_err_code(165)
interrupt_handler_no_err_code(166)
interrupt_handler_no_err_code(167)
interrupt_handler_no_err_code(168)
interrupt_handler_no_err_code(169)
interrupt_handler_no_err_code(170)
interrupt_handler_no_err_code(171)
interrupt_handler_no_err_code(172)
interrupt_handler_no_err_code(173)
interrupt_handler_no_err_code(174)
interrupt_handler_no_err_code(175)
interrupt_handler_no_err_code(176)
interrupt_handler_no_err_code(177)
interrupt_handler_no_err_code(178)
interrupt_handler_no_err_code(179)
interrupt_handler_no_err_code(180)
interrupt_handler_no_err_code(181)
interrupt_handler_no_err_code(182)
interrupt_handler_no_err_code(183)
interrupt_handler_no_err_code(184)
interrupt_handler_no_err_code(185)
interrupt_handler_no_err_code(186)
interrupt_handler_no_err_code(187)
interrupt_handler_no_err_code(188)
interrupt_handler_no_err_code(189)
interrupt_handler_no_err_code(190)
interrupt_handler_no_err_code(191)
interrupt_handler_no_err_code(192)
interrupt_handler_no_err_code(193)
interrupt_handler_no_err_code(194)
interrupt_handler_no_err_code(195)
interrupt_handler_no_err_code(196)
interrupt_handler_no_err_code(197)
interrupt_handler_no_err_code(198)
interrupt_handler_no_err_code(199)
interrupt_handler_no_err_code(200)
interrupt_handler_no_err_code(201)
interrupt_handler_no_err_code(202)
interrupt_handler_no_err_code(203)
interrupt_handler_no_err_code(204)
interrupt_handler_no_err_code(205)
interrupt_handler_no_err_code(206)
interrupt_handler_no_err_code(207)
interrupt_handler_no_err_code(208)
interrupt_handler_no_err_code(209)
interrupt_handler_no_err_code(210)
interrupt_handler_no_err_code(211)
interrupt_handler_no_err_code(212)
interrupt_handler_no_err_code(213)
interrupt_handler_no_err_code(214)
interrupt_handler_no_err_code(215)
interrupt_handler_no_err_code(216)
interrupt_handler_no_err_code(217)
interrupt_handler_no_err_code(218)
interrupt_handler_no_err_code(219)
interrupt_handler_no_err_code(220)
interrupt_handler_no_err_code(221)
interrupt_handler_no_err_code(222)
interrupt_handler_no_err_code(223)
interrupt_handler_no_err_code(224)
interrupt_handler_no_err_code(225)
interrupt_handler_no_err_code(226)
interrupt_handler_no_err_code(227)
interrupt_handler_no_err_code(228)
interrupt_handler_no_err_code(229)
interrupt_handler_no_err_code(230)
interrupt_handler_no_err_code(231)
interrupt_handler_no_err_code(232)
interrupt_handler_no_err_code(233)
interrupt_handler_no_err_code(234)
interrupt_handler_no_err_code(235)
interrupt_handler_no_err_code(236)
interrupt_handler_no_err_code(237)
interrupt_handler_no_err_code(238)
interrupt_handler_no_err_code(239)
interrupt_handler_no_err_code(240)
interrupt_handler_no_err_code(241)
interrupt_handler_no_err_code(242)
interrupt_handler_no_err_code(243)
interrupt_handler_no_err_code(244)
interrupt_handler_no_err_code(245)
interrupt_handler_no_err_code(246)
interrupt_handler_no_err_code(247)
interrupt_handler_no_err_code(248)
interrupt_handler_no_err_code(249)
interrupt_handler_no_err_code(250)
interrupt_handler_no_err_code(251)
interrupt_handler_no_err_code(252)
interrupt_handler_no_err_code(253)
interrupt_handler_no_err_code(254)
interrupt_handler_no_err_code(255)
