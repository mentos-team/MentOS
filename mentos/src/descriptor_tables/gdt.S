;                MentOS, The Mentoring Operating system project
; @file   gdt.asm
; @brief
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

global gdt_flush        ; Allows the C code to call gdt_flush().

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

gdt_flush:
    mov eax, [esp+4] ; Get the pointer to the GDT, passed as a parameter.
    lgdt [eax]       ; Load the new GDT pointer

    ; The data segments selectors (registers), can be easily modified using
    ; simple mov instruction, but the cs can't be used with mov, so you use:
    jmp 0x08:flush
    ; to load the segment configurations into the the code segment selector.

flush:
    mov ax, 0x10     ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax       ; Load all data segment selectors
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret