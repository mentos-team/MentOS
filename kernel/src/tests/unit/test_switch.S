;                MentOS, The Mentoring operating system project
; @file   test_switch.S
; @brief  Raw round-trip test for the kernel context switch primitive.
; @copyright (c) 2014-2024 This file is distributed under the MIT License.
; See LICENSE.md for details.

section .bss
align 16
switch_test_stack: resb 128
switch_test_stack_end:
align 4
switch_test_prev: resd 1
switch_test_next: resd 1

section .text

extern switch_to
global switch_to_round_trip

; int switch_to_round_trip(void)
; Build a synthetic inactive context, switch to it, and switch back to the
; caller's continuation. This exercises the exact stack contract without
; involving scheduler state or a task address space.
switch_to_round_trip:
    push ebp
    mov ebp, esp

    mov edx, switch_test_stack_end
    sub edx, 20
    mov dword [edx + 0], 0
    mov dword [edx + 4], 0
    mov dword [edx + 8], 0
    mov dword [edx + 12], 0
    mov dword [edx + 16], switch_test_resumed

    push edx
    push switch_test_prev
    call switch_to
    add esp, 8

    mov eax, 1
    pop ebp
    ret

switch_test_resumed:
    mov eax, [switch_test_prev]
    push eax
    push switch_test_next
    call switch_to
    ud2
