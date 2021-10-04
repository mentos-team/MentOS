;                MentOS, The Mentoring Operating system project
; @file   tss.asm
; @brief
; @copyright (c) 2014-2021 This file is distributed under the MIT License.
; See LICENSE.md for details.

global tss_flush

; -----------------------------------------------------------------------------
; SECTION (text)
; -----------------------------------------------------------------------------
section .text

tss_flush:
    mov ax, 0x28
    ltr ax
    ret
