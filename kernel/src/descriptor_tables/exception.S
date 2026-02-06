;                MentOS, The Mentoring Operating system project
; @file   exception.asm
; @brief
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

extern isr_handler

; Unified macro for all ISRs - takes interrupt number and error code flag.
; 2nd parameter: has_error_code (1 if CPU pushes error code, 0 if not)
%macro ISR 2
    global INT_%1
    INT_%1:
        cli
        %if %2 == 0
            ; CPU didn't push error code, push dummy for uniform stack frame
            push 0
        %endif
        push %1
        jmp isr_common
%endmacro

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

; Standard X86 interrupt service routines
ISR 0, 0
ISR 1, 0
ISR 2, 0
ISR 3, 0
ISR 4, 0
ISR 5, 0
ISR 6, 0
ISR 7, 0
ISR 8, 1
ISR 9, 0
ISR 10, 1
ISR 11, 1
ISR 12, 1
ISR 13, 1
ISR 14, 1
ISR 15, 0
ISR 16, 0
ISR 17, 0
ISR 18, 0
ISR 19, 0
ISR 20, 0
ISR 21, 0
ISR 22, 0
ISR 23, 0
ISR 24, 0
ISR 25, 0
ISR 26, 0
ISR 27, 0
ISR 28, 0
ISR 29, 0
ISR 30, 0
ISR 31, 0

ISR 80, 0

isr_common:
    ; Save all registers (eax, ecx, edx, ebx, esp, ebp, esi, edi)
    pusha

    ; Save segment registers
    push ds
    push es
    push fs
    push gs

    ; Normalize stack frame: CPU only pushes SS/UESP on privilege level change.
    ; Check if we came from user mode (ring 3) by examining CS on stack.
    ; Stack layout at this point:
    ;   [ESP+0]=GS, [ESP+4]=FS, [ESP+8]=ES, [ESP+12]=DS, [ESP+16]=EDI, [ESP+20]=ESI,
    ;   [ESP+24]=EBP, [ESP+28]=ESP, [ESP+32]=EBX, [ESP+36]=EDX, [ESP+40]=ECX, [ESP+44]=EAX,
    ;   [ESP+48]=INT_NO, [ESP+52]=ERR_CODE, [ESP+56]=EIP, [ESP+60]=CS, [ESP+64]=EFLAGS, [ESP+68/72]=UESP/SS
    mov eax, [esp + 60]         ; Load CS (offset: 60 bytes from current ESP)
    test eax, 0x3               ; Check privilege bits (ring 0 = 0x00, ring 3 = 0x03)
    jnz .from_user_mode

    ; From kernel mode - CPU didn't push SS/UESP, so we must push them
    ; to maintain consistent pt_regs structure
    mov eax, 0x10               ; Kernel data segment selector
    push eax                    ; Push as SS
    mov eax, [esp + 60]         ; Re-load original ESP before our pushes
    add eax, 8                  ; Account for INT_NO and ERR_CODE on stack
    push eax                    ; Push as UESP
    jmp .frame_normalized

.from_user_mode:
    ; From user mode - CPU already pushed SS/UESP, they're already on stack
    ; No action needed, frame is already normalized

.frame_normalized:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    cld

    ; Call the interrupt handler.
    push    esp
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

    ; Pop SS and UESP (normalized frame)
    add esp, 0x8

    iret                        ; pops 5 things at once:
                                ;   CS, EIP, EFLAGS, SS, and ESP

; -----------------------------------------------------------------------------
; SECTION (note) - Inform the linker that the stack does not need to be executable
; -----------------------------------------------------------------------------
section .note.GNU-stack
