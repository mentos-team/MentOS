;                MentOS, The Mentoring Operating system project
; @file switch.S
; @brief This file contains the context switch code.
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

global context_switch

; void context_switch(struct context *old, struct context *new);
; 
; @brief Perform a low-level context switch between two task contexts.
; @param old Pointer to the current task's context to save.
; @param new Pointer to the next task's context to restore.
;
; This function saves the current register state of the calling task into `old`
; and restores the register state from `new`, effectively switching execution
; to another task.
;
; At entry, the stack contains the return address and arguments:
;     [esp + 0]  → return address
;     [esp + 4]  → old (task_context_t*)
;     [esp + 8]  → new (task_context_t*)
;
; However, since `pusha` is used at the start of this function to save all
; general-purpose registers, the stack pointer `esp` moves down by 32 bytes
; (8 registers * 4 bytes each). To access the original arguments again,
; we use:
;     lea edx, [esp + 32]     // edx now points to the caller's original esp
;     mov eax, [edx + 32 + 4] // old
;     mov ebx, [edx + 32 + 8] // new
;
; This technique ensures compatibility with the cdecl calling convention
; and avoids corrupting the stack layout while preserving the register state.
;
context_switch:
    ; Save the ESP before pusha.
    lea edx, [esp + 32]
    ; Push the whole context.
    pusha
    ; Get the current context.
    mov eax, [esp + 32 + 4]     ; old
    ; Check if the context is NULL.
    test eax, eax
    jz .skip_save

    ; Store to struct in the same order as they appear on stack.
    mov ecx, [esp + 32]         ; old.eip
    mov [eax + 32], ecx

    mov ecx, [esp + 28]         ; old.edi
    mov [eax + 0], ecx

    mov ecx, [esp + 24]         ; old.esi
    mov [eax + 4], ecx

    mov ecx, [esp + 20]         ; old.ebp
    mov [eax + 8], ecx
    
    mov [eax + 12], edx         ; old.esp (saved before pusha)

    mov ecx, [esp + 12]         ; old.ebx
    mov [eax + 16], ecx

    mov ecx, [esp + 8]          ; old.edx
    mov [eax + 20], ecx

    mov ecx, [esp + 4]          ; old.ecx
    mov [eax + 24], ecx

    mov ecx, [esp + 0]          ; old.eax
    mov [eax + 28], ecx

.skip_save:
    ; Get the pointer to the new context.
    mov eax, [esp + 32 + 8]     ; new
    ; Restore the context.
    mov esp, [eax + 12]         ; new.esp
    ; jump to the return address.
    jmp .resume_execution

.resume_execution:
    popa
    pop ebx
    jmp ebx

; -----------------------------------------------------------------------------
; SECTION (note) - Inform the linker that the stack does not need to be executable
; -----------------------------------------------------------------------------
section .note.GNU-stack
