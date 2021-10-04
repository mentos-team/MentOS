;                MentOS, The Mentoring Operating system project
; @file   idt.asm
; @brief
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

global idt_flush        ; Allows the C code to call idt_flush().

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

idt_flush:
    mov  eax, [esp+4]   ; Get the pointer to the IDT, passed as a parameter.
    lidt [eax]          ; Load the IDT pointer.
    ret