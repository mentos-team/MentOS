;                MentOS, The Mentoring Operating system project
; @file   exception.asm
; @brief
; @copyright (c) 2019 This file is distributed under the MIT License.
; See LICENSE.md for details.

; Macro used to define a ISR which does not push an error code.
%macro ISR_NOERR 1
    [GLOBAL INT_%1]
    INT_%1:
        cli
        ; A normal ISR stub that pops a dummy error code to keep a
        ; uniform stack frame
        push 0
        push %1
        jmp isr_common
%endmacro

; Macro used to define a ISR which pushes an error code.
%macro ISR_ERR 1
    [GLOBAL INT_%1]
    INT_%1:
        cli
        push %1
        jmp isr_common
%endmacro

; Standard X86 interrupt service routines
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19

ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

ISR_NOERR 80

[EXTERN isr_handler]

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
    ; CLD - Azzera la flag di Direzione
    ; Questa istruzione forza semplicemente a zero la flag di Direzione.
    ; Quando la flag di direzione vale 0 tutte le istruzioni per la
    ;  manipolazione delle stringhe agiscono in avanti, cioè dagli indirizzi più
    ;  bassi a quelli più alti.
    ; L'istruzione agisce dunque sui puntatori SI e DI producendo su essi un
    ;  autoincremento proporzionale alla dimensione degli operandi trattati.
    ; Le sue caratteristiche sono riassunte nella seguente tabella (leggi le
    ;  istruzioni  Legenda della Tabella):
    cld

    ; Call the interrupt handler.
    push    esp;
    call    isr_handler
    add     esp, 0x4

    ; Restore segment registers.
    pop gs
    pop fs
    pop es
    pop ds

    ; Restore all registers  (eax, ecx, edx, ebx, esp, ebp, esi, edi).
    popa

    ; Cleanup error code and IRQ #
    add esp, 0x8

    iret                        ; pops 5 things at once:
                                ;   CS, EIP, EFLAGS, SS, and ESP
