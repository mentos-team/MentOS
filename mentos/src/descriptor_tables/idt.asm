;                MentOS, The Mentoring Operating system project
; @file   idt.asm
; @brief
; @copyright (c) 2019 This file is distributed under the MIT License.
; See LICENSE.md for details.

[GLOBAL idt_flush]      ; Allows the C code to call idt_flush().

idt_flush:
    mov  eax, [esp+4]   ; Get the pointer to the IDT, passed as a parameter.
    lidt [eax]          ; Load the IDT pointer.
    ret