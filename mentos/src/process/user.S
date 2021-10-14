;                MentOS, The Mentoring Operating system project
; @file   user.asm
; @brief
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

; Enter userspace (ring3) (from Ring 0, namely Kernel)
; Usage:  enter_userspace(uintptr_t location, uintptr_t stack);
; On stack
; |     stack      | [ebp + 0x0C] ARG1
; |    location    | [ebp + 0x08] ARG0
; | return address | [ebp + 0x04]
; |      EBP       | [ebp + 0x00]
; |      SS        |
; |      ESP       |
; |    EFLAGS      |
; |      CS        |
; |      EIP       |

; We can use the following macros to access the arguments ONLY AFTER 0x23 is
; pushed onto the stack, in fact, the first argument is after 0x08 because
; we just pushed first `ebp` and then `0x23`.
%define ARG0 [ebp + 0x08] ; Argument 0
%define ARG1 [ebp + 0x0C] ; Argument 1
%define ARG2 [ebp + 0x10] ; Argument 2
%define ARG3 [ebp + 0x14] ; Argument 3
%define ARG4 [ebp + 0x18] ; Argument 4

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

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
    mov eax, ARG1           ; get uintptr_t stack
    push eax                ; push process's stack address on Kernel's stack
    ;---------------------------------------------------------------------------

    ;==== (EFLAGS) =============================================================
    pushf                   ; push EFLAGS into Kernel's stack
    pop eax                 ; pop EFLAGS into eax
    or eax, 0x200           ; enable interrupt
    push eax                ; push new EFLAGS on Kernel's stack
    ;---------------------------------------------------------------------------

    ;==== (CS) Code Segment ====================================================
    push 0x1b               ;
    ;---------------------------------------------------------------------------

    ;==== (EIP) Entry point ====================================================
    mov eax, ARG0           ; get uintptr_t location
    push eax                ; push uintptr_t location on Kernel's stack
    ;---------------------------------------------------------------------------

    iret                   ; interrupt return
    pop ebp
    ret

    ; WE SHOULD NOT STILL BE HERE! :(p