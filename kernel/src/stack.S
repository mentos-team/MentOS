;                MentOS, The Mentoring Operating system project
; @file stack.S
; @brief Kernel stack allocation.
; @copyright (c) 2014-2024 This file is distributed under the MIT License.
; See LICENSE.md for details.

bits 32

; Reserve (4*1024*1024) for the stack on a doubleword boundary (increased from 1MB for debug logging)
KERNEL_STACK_SIZE   equ 0x400000

; =============================================================================
; SECTION (bss)
; =============================================================================
section .bss
align 16

global stack_bottom
stack_bottom:
    resb KERNEL_STACK_SIZE

global stack_top
stack_top:
    ; the top of the stack is the bottom because the stack counts down

; =============================================================================
; SECTION (note) - Inform the linker that the stack does not need to be executable
; =============================================================================
section .note.GNU-stack
