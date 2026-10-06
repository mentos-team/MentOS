;                MentOS, The Mentoring operating system project
; @file   switch.S
; @brief  Minimal i386 kernel context switch primitive.
; @copyright (c) 2014-2024 This file is distributed under the MIT License.
; See LICENSE.md for details.

section .text

global switch_to
global ret_from_fork
global enter_prepared_user

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

; First-run return trampoline. ESP points at the complete pt_regs_t built by
; the task constructor. Keep this epilogue byte-for-byte compatible with the
; common interrupt return path.
ret_from_fork:
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8              ; int_no and err_code
    iretd

; One-way first-task handoff. The caller has already set TSS.esp0 and passes
; the address of a complete pt_regs_t; ret_from_fork performs the final iret.
enter_prepared_user:
    mov eax, [esp + 4]
    mov esp, eax
    jmp ret_from_fork

section .note.GNU-stack noalloc noexec nowrite progbits
