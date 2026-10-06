;                MentOS, The Mentoring operating system project
; @file   switch.S
; @brief  Minimal i386 kernel context switch primitive.
; @copyright (c) 2014-2024 This file is distributed under the MIT License.
; See LICENSE.md for details.

section .text

global switch_to

; void switch_to(uint32_t *prev_esp, uint32_t next_esp)
;
; The caller disables interrupts and clears DF. Only callee-saved integer
; registers belong to this primitive; the continuation restores the caller's
; remaining state from its interrupt/syscall frame.
switch_to:
    mov eax, [esp + 4]       ; outgoing ESP destination
    mov edx, [esp + 8]       ; incoming ESP

    push ebx
    push esi
    push edi
    push ebp
    mov [eax], esp

    mov esp, edx
    pop ebp
    pop edi
    pop esi
    pop ebx
    ret
