;                MentOS, The Mentoring Operating system project
; @file   tss.asm
; @brief
; @copyright (c) 2019 This file is distributed under the MIT License.
; See LICENSE.md for details.

[GLOBAL tss_flush]

tss_flush:
    mov ax, 0x28
    ltr ax
    ret
