;                MentOS, The Mentoring Operating system project
; @file boot.asm
; @brief Kernel start location, multiboot header
; @copyright (c) 2019 This file is distributed under the MIT License.
; See LICENSE.md for details.

[BITS 32]       ; All instructions should be 32-bit.
[EXTERN kmain]  ; The start point of our C code

; Grub is informed with this flag to load
; the kernel and kernel modules on a page boundary.
MBOOT_PAGE_ALIGN    equ 1<<0
; Grub is informed with this flag to provide the kernel
; with memory information.
MBOOT_MEM_INFO      equ 1<<1
; This is the multiboot magic value.
MBOOT_HEADER_MAGIC  equ 0x1BADB002
; This is the flag combination that we prepare for Grub
; to read at kernel load time.
MBOOT_HEADER_FLAGS  equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
; Grub reads this value to make sure it loads a kernel
; and not just garbage.
MBOOT_CHECKSUM      equ - (MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

LOAD_MEMORY_ADDRESS equ 0x00000000
; reserve (1024*1024) for the stack on a doubleword boundary
KERNEL_STACK_SIZE   equ 0x100000

; -----------------------------------------------------------------------------
; SECTION (multiboot_header)
; -----------------------------------------------------------------------------
section .multiboot_header
align 4
; This is the GRUB Multiboot header.
multiboot_header:
    dd MBOOT_HEADER_MAGIC
    dd MBOOT_HEADER_FLAGS
    dd MBOOT_CHECKSUM

; -----------------------------------------------------------------------------
; SECTION (data)
; -----------------------------------------------------------------------------
section .data, nobits
align 4096
[GLOBAL boot_page_dir]
boot_page_dir:
    resb 0x1000
boot_page_tabl:
    resb 0x1000

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text
[GLOBAL kernel_entry]
kernel_entry:
    ; Clear interrupt flag [IF = 0]; 0xFA
    cli
	; To set up a stack, we simply set the esp register to point to the top of
	; our stack (as it grows downwards).
    mov esp, stack_top
    ; pass the initial ESP
    push esp
    ;mov ebp, esp
    ; pass Multiboot info structure
    push ebx
    ; pass Multiboot magic number
    push eax
    ; Call the kmain() function inside kernel.c
    call kmain
    ; Set interrupt flag [IF = 1]; 0xFA
    ; Clear interrupts and hang if we return from kmain
    cli
hang:
    hlt
    jmp hang


; -----------------------------------------------------------------------------
; SECTION (bss)
; -----------------------------------------------------------------------------
section .bss
[GLOBAL stack_bottom]
[GLOBAL stack_top]
align 16
stack_bottom:
    resb KERNEL_STACK_SIZE
stack_top:
    ; the top of the stack is the bottom because the stack counts down
