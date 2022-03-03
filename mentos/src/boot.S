;                MentOS, The Mentoring Operating system project
; @file boot.asm
; @brief Kernel start location, multiboot header
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

bits 32             ; All instructions should be 32-bit.
extern boot_main    ; The start point of our C code

; The magic field should contain this.
MULTIBOOT_HEADER_MAGIC     equ 0x1BADB002
; This should be in %eax.
MULTIBOOT_BOOTLOADER_MAGIC equ 0x2BADB002

; = Specify what GRUB should PROVIDE =========================================
; Align the kernel and kernel modules on i386 page (4KB) boundaries.
MULTIBOOT_PAGE_ALIGN  equ 0x00000001
; Provide the kernel with memory information.
MULTIBOOT_MEMORY_INFO equ 0x00000002
; Must pass video information to OS.
MULTIBOOT_VIDEO_MODE  equ 0x00000004
; -----------------------------------------------------------------------------

; This is the flag combination that we prepare for Grub
; to read at kernel load time.
MULTIBOOT_HEADER_FLAGS  equ (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)
; Grub reads this value to make sure it loads a kernel
; and not just garbage.
MULTIBOOT_CHECKSUM      equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

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
    ; magic
    dd MULTIBOOT_HEADER_MAGIC
    ; flags
    dd MULTIBOOT_HEADER_FLAGS
    ; checksum
    dd MULTIBOOT_CHECKSUM

; -----------------------------------------------------------------------------
; SECTION (data)
; -----------------------------------------------------------------------------
; section .data nobits
; align 4096

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text
global boot_entry
boot_entry:
    ; Clear interrupt flag [IF = 0]; 0xFA
    cli
    ; To set up a stack, we simply set the esp register to point to the top of
    ; our stack (as it grows downwards).
    mov esp, stack_top
    ; pass the initial ESP
    push esp
    ; pass Multiboot info structure
    push ebx
    ; pass Multiboot magic number
    push eax
    ; Call the boot_main() function inside boot.c
    call boot_main
    ; Set interrupt flag [IF = 1]; 0xFA
    ; Clear interrupts and hang if we return from boot_main
    cli
hang:
    hlt
    jmp hang

global boot_kernel
boot_kernel:
    mov edx, [esp + 4] ; stack_pointer
    mov ebx, [esp + 8] ; entry
    mov eax, [esp + 12] ; boot info
    mov esp, edx ; set stack pointer
    push eax ; push the boot info
    call ebx ; call the kernel main


; -----------------------------------------------------------------------------
; SECTION (bss)
; -----------------------------------------------------------------------------
section .bss
align 16

global stack_bottom
stack_bottom:
    resb KERNEL_STACK_SIZE

global stack_top
stack_top:
    ; the top of the stack is the bottom because the stack counts down
