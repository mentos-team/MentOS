;                MentOS, The Mentoring Operating system project
; @file   exception.asm
; @brief
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

extern isr_handler

; Unified macro for all ISRs - takes interrupt number and error code flag.
; 2nd parameter: has_error_code (1 if CPU pushes error code, 0 if not)
%macro ISR 2
    global INT_%1
    INT_%1:
        cli
        %if %2 == 0
            ; CPU didn't push error code, push dummy for uniform stack frame
            push 0
        %endif
        push %1
        jmp isr_common
%endmacro

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

; Standard X86 interrupt service routines
ISR 0, 0
ISR 1, 0
ISR 2, 0
ISR 3, 0
ISR 4, 0
ISR 5, 0
ISR 6, 0
ISR 7, 0
ISR 8, 1
ISR 9, 0
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 15, 0
ISR 16, 0
ISR 17, 0
ISR 18, 0
ISR 19, 0
ISR 20, 0
ISR 21, 0
ISR 22, 0
ISR 23, 0
ISR 24, 0
ISR 25, 0
ISR 26, 0
ISR 27, 0
ISR 28, 0
ISR 29, 0
ISR 30, 0
ISR 31, 0

ISR 80, 0

isr_common:
    ; Save all registers (eax, ecx, edx, ebx, esp, ebp, esi, edi)
    pusha

    ; Save segment registers
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld

    ; Call the interrupt handler.
    push    esp
    call    isr_handler
    add     esp, 0x4

    ; Restore segment registers.
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore all registers (eax, ecx, edx, ebx, esp, ebp, esi, edi).
    popa

    ; Cleanup error code and IRQ #
    add esp, 0x8

    iret                        ; pops 5 things at once:
                                ;   CS, EIP, EFLAGS, SS, and ESP

; -----------------------------------------------------------------------------
; SECTION (note) - Inform the linker that the stack does not need to be executable
; -----------------------------------------------------------------------------
section .note.GNU-stack
