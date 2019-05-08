///                MentOS, The Mentoring Operating system project
/// @file   kernel.h
/// @brief  Kernel generic data structure and functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "elf.h"
#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "multiboot.h"

/// The maximum length of a file name.
#define MAX_FILENAME_LENGTH 64

/// The maximum length of a path.
#define MAX_PATH_LENGTH 256

/// The maximum number of modules.
#define MAX_MODULES 10

/// This should be in %eax.
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/// Our kernel now loads at 0xC0000000, so what low memory address such as
/// 0xb800 you used to access, should be LOAD_MEMORY_ADDRESS + 0xb800
#define LOAD_MEMORY_ADDRESS 0x00000000

// TODO: doxygen comment.
extern uintptr_t initial_esp;

/// @brief Halt.
inline static void halt()
{
	__asm__ __volatile__("hlt" ::: "memory");
}

/// @brief Pause.
inline static void pause()
{
	__asm__ __volatile__("pause" ::: "memory");
}

#define K 1024

#define M (1024 * K)

#define G (1024 * M)

/// Pointer the beging of the module.
extern char *module_start[MAX_MODULES];

/// Address to the end of the module.
extern char *module_end[MAX_MODULES];

/// Elf symbols of the kernel.
extern elf_symbols_t kernel_elf;

/// @brief                      Entry point of the kernel.
/// @param boot_informations    Information concerning the boot.
/// @return                     The exit status of the kernel.
int kmain(uint32_t magic, multiboot_info_t *header, uintptr_t esp);

/// @brief Register structs for interrupt/exception.
typedef struct register_t {
	/// FS and GS have no hardware-assigned uses.
	uint32_t gs;
	/// FS and GS have no hardware-assigned uses.
	uint32_t fs;
	/// Extra Segment determined by the programmer.
	uint32_t es;
	/// Data Segment.
	uint32_t ds;
	/// 32-bit destination register.
	uint32_t edi;
	/// 32-bit source register.
	uint32_t esi;
	/// 32-bit base pointer register.
	uint32_t ebp;
	/// 32-bit stack pointer register.
	uint32_t esp;
	/// 32-bit base register.
	uint32_t ebx;
	/// 32-bit data register.
	uint32_t edx;
	/// 32-bit counter.
	uint32_t ecx;
	/// 32-bit accumulator register.
	uint32_t eax;
	/// Interrupt number.
	uint32_t int_no;
	/// Error code.
	uint32_t err_code;
	/// Instruction Pointer Register.
	uint32_t eip;
	/// Code Segment.
	uint32_t cs;
	/// 32-bit flag register.
	uint32_t eflags;
	// TODO: Check meaning!
	uint32_t useresp;
	/// Stack Segment.
	uint32_t ss;
} register_t;

/*
 * /// @brief Register structs for bios service.
 * typedef struct register16_t
 * {
 *      /// Destination Index.
 *      uint16_t di;
 *      /// Source Index.
 *      uint16_t si;
 *      /// Base Pointer.
 *      uint16_t bp;
 *      /// Stack Pointer.
 *      uint16_t sp;
 *      /// Also known as the base register.
 *      uint16_t bx;
 *      /// Also known as the data register.
 *      uint16_t dx;
 *      /// Also known as the count register.
 *      uint16_t cx;
 *      /// Is the primary accumulator.
 *      uint16_t ax;
 *      /// Data Segment.
 *      uint16_t ds;
 *      /// Extra Segment determined by the programmer.
 *      uint16_t es;
 *      /// FS and GS have no hardware-assigned uses.
 *      uint16_t fs;
 *      /// FS and GS have no hardware-assigned uses.
 *      uint16_t gs;
 *      /// Stack Segment.
 *      uint16_t ss;
 *      /// 32-bit flag register.
 *      uint16_t eflags;
 * } register16_t;
 */

//==== Interrupt stack frame ===================================================
// Interrupt stack frame. When the CPU moves from Ring3 to Ring0 because of
// an interrupt, the following registes/values are moved into the kernel's stack
// TODO: doxygen comment.
typedef struct pt_regs {
	/// FS and GS have no hardware-assigned uses.
	uint32_t gs;
	/// FS and GS have no hardware-assigned uses.
	uint32_t fs;
	/// Extra Segment determined by the programmer.
	uint32_t es;
	/// Data Segment.
	uint32_t ds;
	/// 32-bit destination register.
	uint32_t edi;
	/// 32-bit source register.
	uint32_t esi;
	/// 32-bit base pointer register.
	uint32_t ebp;
	/// 32-bit stack pointer register.
	uint32_t esp;
	/// 32-bit base register.
	uint32_t ebx;
	/// 32-bit data register.
	uint32_t edx;
	/// 32-bit counter.
	uint32_t ecx;
	/// 32-bit accumulator register.
	uint32_t eax;
	/// Interrupt number.
	uint32_t int_no;
	/// Error code.
	uint32_t err_code;
	/// Instruction Pointer Register.
	uint32_t eip;
	/// Code Segment.
	uint32_t cs;
	/// 32-bit flag register.
	uint32_t eflags;
	// TODO: Check meaning!
	uint32_t useresp;
	/// Stack Segment.
	uint32_t ss;
} pt_regs;
//==============================================================================

//==== Floating Point Unit (FPU) Register ======================================
// Data structure used to save FPU registers.
/// @brief Environment information of floating point unit.
typedef struct {
	/// Control word (16bits).
	long en_cw;
	/// Status word (16bits).
	long en_sw;
	/// Tag word (16bits).
	long en_tw;
	/// Floating point instruction pointer.
	long en_fip;
	/// Floating code segment selector.
	unsigned short en_fcs;
	/// Opcode last executed (11 bits).
	unsigned short en_opcode;
	/// Floating operand offset.
	long en_foo;
	/// Floating operand segment selector.
	long en_fos;
} env87;

/// @brief Contents of each floating point accumulator.
typedef struct {
	unsigned char fp_bytes[10];
} fpacc87;

/// @brief Floating point context.
typedef struct {
	/// Floating point control/status.
	env87 sv_env;
	/// Accumulator contents, 0-7.
	fpacc87 sv_ac[8];
	/// Padding for (now unused) saved status word.
	unsigned char sv_pad0[4];
	/*
     * Bogus padding for emulators. Emulators should use their own
     * struct and arrange to store into this struct (ending here)
     * before it is inspected for ptracing or for core dumps. Some
     * emulators overwrite the whole struct. We have no good way of
     * knowing how much padding to leave. Leave just enough for the
     * GPL emulator's i387_union (176 bytes total).
     */
	unsigned char sv_pad[64]; // Padding; used by emulators
} save87;

// TODO: doxygen comment.
typedef struct {
	/// Control word (16bits).
	uint16_t en_cw;
	/// Status word (16bits).
	uint16_t en_sw;
	/// Tag word (16bits).
	uint16_t en_tw;
	/// Opcode last executed (11 bits).
	uint16_t en_opcode;
	/// Floating point instruction pointer.
	uint32_t en_fip;
	/// Floating code segment selector.
	uint16_t en_fcs;
	/// Padding.
	uint16_t en_pad0;
	/// Floating operand offset.
	uint32_t en_foo;
	/// Floating operand segment selector.
	uint16_t en_fos;
	/// Padding.
	uint16_t en_pad1;
	/// SSE sontorol/status register.
	uint32_t en_mxcsr;
	/// Valid bits in mxcsr.
	uint32_t en_mxcsr_mask;
} envxmm;

/// @brief Contents of each SSE extended accumulator.
typedef struct {
	unsigned char xmm_bytes[16];
} xmmacc;

// TODO: doxygen comment.
typedef struct {
	envxmm sv_env;
	struct {
		fpacc87 fp_acc;
		/// Padding.
		unsigned char fp_pad[6];
	} sv_fp[8];
	xmmacc sv_xmm[8];

	/// Padding.
	unsigned char sv_pad[224];
} __attribute__((__aligned__(16))) savexmm;

// TODO: doxygen comment.
typedef union {
	save87 sv_87;
	savexmm sv_xmm;
} savefpu;
//==============================================================================
