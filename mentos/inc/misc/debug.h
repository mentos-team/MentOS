///                MentOS, The Mentoring Operating system project
/// @file debug.h
/// @brief Debugging primitives.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "port_io.h"

/// @brief Used to enable the device. Any I/O to the debug module before this
/// command is sent will simply be ignored.
#define DBG_ENABLE 0x8A00

/// @brief Disable the I/O interface to the debugger and the memory
/// monitoring functions.
#define DBG_DISABLE 0x8AFF

/// @brief Selects register 0: Memory monitoring range start address
/// (inclusive).
#define SELECTS_REG_0 0x8A01

/// @brief Selects register 1: Memory monitoring range end address (exclusive).
#define SELECTS_REG_1 0x8A02

/// @brief Enable address range memory monitoring as indicated by register 0
/// and 1 and clears both registers.
#define ENABLE_ADDR_RANGE_MEM_MONITOR 0x8A80

/// @brief If the debugger is enabled (via --enable-debugger), sending 0x8AE2
/// to port 0x8A00 after the device has been enabled will enable instruction
/// tracing.
#define INSTRUCTION_TRACE_ENABLE 0x8AE3

/// @brief If the debugger is enabled (via --enable-debugger), sending 0x8AE2
/// to port 0x8A00 after the device has been enabled will disable instruction
/// tracing.
#define INSTRUCTION_TRACE_DISABLE 0x8AE2

/// @brief If the debugger is enabled (via --enable-debugger), sending 0x8AE5
/// to port 0x8A00 after the device has been enabled will enable register
/// tracing. This currently output the value of all the registers for each
/// instruction traced. Note: instruction tracing must be enabled to view
/// the register tracing.
#define REGISTER_TRACE_ENABLE 0x8AE5

/// @brief If the debugger is enabled (via --enable-debugger), sending 0x8AE4
/// to port 0x8A00 after the device has been enabled will disable register
/// tracing.
#define REGISTER_TRACE_DISABLE 0x8AE4

/// @brief Prints the given string to the debug output.
void _dbg_print(const char *file, const char *fun, int line, const char *msg,
				...);

#define __FILENAME__                                                           \
	(__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : \
										__FILE__)

#define dbg_print(...) _dbg_print(__FILENAME__, __func__, __LINE__, __VA_ARGS__)

/// @brief Prints the given registers to the debug output.
void print_reg(register_t *reg);

/// @brief Prints the given intrframe to the debug output.
void print_intrframe(pt_regs *frame);
