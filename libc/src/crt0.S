extern main
extern __libc_start_main
global _start

; Ensure _start is placed at the beginning of .text section
; to have consistent entry point offset regardless of optimization level
section .text.entry
_start:                     ; _start is the entry point known to the linker
    mov ebp, 0              ; Set ebp to 0 as x86 programs require
    push main               ; Push the pointer to `main` to the stack.
    call __libc_start_main  ; Call the libc initialization function.
    mov ebx, eax            ; Move `main` return value to ebx.
    mov eax, 1              ; Call the `exit` function by using `int 80` (i.e., a system call)
    int 0x80

; Rest of text section
section .text

; -----------------------------------------------------------------------------
; SECTION (note) - Inform the linker that the stack does not need to be executable
; -----------------------------------------------------------------------------
section .note.GNU-stack
