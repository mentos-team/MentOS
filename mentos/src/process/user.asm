;                MentOS, The Mentoring Operating system project
; @file   user.asm
; @brief
; @copyright (c) 2019 This file is distributed under the MIT License.
; See LICENSE.md for details.

; Enter userspace (ring3) (from Ring 0, namely Kernel)
; Usage:  enter_userspace(uintptr_t location, uintptr_t stack);
; On stack
; |     stack      |
; |    location    |
; | return address |
; |      EBP       | EBP
; |      SS        |
; |      ESP       |
; |    EFLAGS      |
; |      CS        |
; |      EIP       |

global enter_userspace      ; Allows the C code to call enter_userspace(...).
enter_userspace:

    push ebp                ; Save current ebp
    mov ebp, esp            ; open a new stack frame

    ;==== Segment selector =====================================================
    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; we don't need to worry about SS. it's handled by iret
    ;---------------------------------------------------------------------------

    ; We have to prepare the following stack before executing iret
    ;    SS           --> Segment selector
    ;    ESP          --> Stack address
    ;    EFLAGS       --> CPU state flgas
    ;    CS           --> Code segment
    ;    EIP          --> Entry point
    ;

    ;==== User data segmenet with bottom 2 bits set for ring3 ?=================
    push 0x23               ; push SS on Kernel's stack
    ;---------------------------------------------------------------------------

    ;==== (ESP) Stack address ==================================================
    mov eax, [ebp + 0xC]    ; get uintptr_t stack
    push eax                ; push process's stack address on Kernel's stack
    ;---------------------------------------------------------------------------

    ;==== (EFLAGS) =============================================================
    pushf                   ; push EFLAGS into Kernel's stack
    pop eax                 ; pop EFLAGS into eax
    or eax, 0x200           ; enable interrupt ?request ring3
    push eax                ; push new EFLAGS on Kernel's stack
    ;---------------------------------------------------------------------------

    ;==== (CS) Code Segment ====================================================
    push 0x1B               ;
    ;---------------------------------------------------------------------------

    ;==== (EIP) Entry point ====================================================
    mov eax, [ebp + 0x8]    ; get uintptr_t location
    push eax                ; push uintptr_t location on Kernel's stack
    ;---------------------------------------------------------------------------

    iret                   ; interrupt return

    ; WE SHOULD NOT STILL BE HERE! :(

    ;==== Reset segment selector ===============================================
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ;---------------------------------------------------------------------------

    add esp, 0x14           ; reset stack pointer (20 bytes)
    pop ebp                 ; reset value of ebp
    ret                     ; return to kernel code
