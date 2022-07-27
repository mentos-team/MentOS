/// @file cpuid.h
/// @brief Structures and functions to handle the CPUID.
/// @details
/// CPUID instruction (identified by a CPUID opcode) is a processor
/// supplementary instruction (its name derived from CPU IDentification)
/// allowing software to discover details of the processor.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdint.h"
#include "kernel.h"

/// Dimension of the exc flags.
#define ECX_FLAGS_SIZE 24

/// Dimension of the edx flags.
#define EDX_FLAGS_SIZE 32

/// @brief Contains the information concerning the CPU.
typedef struct cpuinfo_t {
    /// The name of the vendor.
    char cpu_vendor[13];
    /// The type of the CPU.
    char *cpu_type;
    /// The family of the CPU.
    uint32_t cpu_family;
    /// The model of the CPU.
    uint32_t cpu_model;
    /// Identifier for individual cores when the CPU is interrogated by the
    /// CPUID instruction.
    uint32_t apic_id;
    /// Ecx flags.
    uint32_t cpuid_ecx_flags[ECX_FLAGS_SIZE];
    /// Edx flags.
    uint32_t cpuid_edx_flags[EDX_FLAGS_SIZE];
    /// TODO: Comment.
    int is_brand_string;
    /// TODO: Comment.
    char *brand_string;
} cpuinfo_t;

/// This will be populated with the information concerning the CPU.
cpuinfo_t sinfo;

/// @brief Main CPUID procedure.
/// @param cpuinfo Structure to fill with CPUID information.
void get_cpuid(cpuinfo_t *cpuinfo);

/// @brief Actual CPUID call.
/// @param registers The registers to fill with the result of the call.
void call_cpuid(pt_regs *registers);

/// @brief Extract vendor string.
/// @param cpuinfo   The struct containing the CPUID infos.
/// @param registers The registers.
void cpuid_write_vendor(cpuinfo_t *cpuinfo, pt_regs *registers);

// TODO: doxygen documentation.
/// @brief
/// @param cpuinfo
/// @param registers
/// @details
/// CPUID is called with EAX=1
/// EAX contains Type, Family, Model and Stepping ID
/// EBX contains the Brand Index if supported, and the APIC ID
/// ECX/EDX contains feature information
void cpuid_write_proctype(cpuinfo_t *cpuinfo, pt_regs *registers);

// TODO: doxygen documentation.
/// @brief EAX=1, ECX contains a list of supported features.
/// @param cpuinfo
/// @param ecx
void cpuid_feature_ecx(cpuinfo_t *cpuinfo, uint32_t ecx);

// TODO: doxygen documentation.
/// @brief EAX=1, EDX contains a list of supported features.
/// @param cpuinfo
/// @param edx
void cpuid_feature_edx(cpuinfo_t *cpuinfo, uint32_t edx);

// TODO: doxygen documentation.
/// @brief Extract single byte from a register.
/// @param reg
/// @param position
/// @param value
/// @return
uint32_t cpuid_get_byte(uint32_t reg, uint32_t position, uint32_t value);

/// @brief Index of brand strings. TODO: Document
/// @param f Stack frame.
/// @return The brand string.
char *cpuid_brand_index(pt_regs *f);

/// @brief Brand string is contained in EAX, EBX, ECX and EDX.
/// @param f Stack frame.
/// @return The brand string.
char *cpuid_brand_string(pt_regs *f);
