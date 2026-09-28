/// @file cpuid.h
/// @brief Structures and functions to handle the CPUID.
/// @details
/// CPUID instruction (identified by a CPUID opcode) is a processor
/// supplementary instruction (its name derived from CPU IDentification)
/// allowing software to discover details of the processor.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "kernel.h"
#include "stdint.h"

/// Dimension of the exc flags.
#define ECX_FLAGS_SIZE 24

/// Dimension of the edx flags.
#define EDX_FLAGS_SIZE 32

/// @brief Contains the information concerning the CPU.
typedef struct cpuinfo {
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
    /// Non-zero when the processor reports a full brand string (CPUID leaf
    /// 0x80000002 and up) rather than only a brand index.
    int is_brand_string;
    /// The processor's marketing name, either the full brand string or the
    /// name the brand index resolves to.
    char *brand_string;
} cpuinfo_t;

/// This will be populated with the information concerning the CPU.
cpuinfo_t sinfo;

/// @brief Main CPUID procedure.
/// @param cpuinfo Structure to fill with CPUID information.
void get_cpuid(cpuinfo_t *cpuinfo);

/// @brief Actual CPUID call.
/// @param registers The registers to fill with the result of the call.
void call_cpuid(pt_regs_t *registers);

/// @brief Extract vendor string.
/// @param cpuinfo   The struct containing the CPUID infos.
/// @param registers The registers.
void cpuid_write_vendor(cpuinfo_t *cpuinfo, pt_regs_t *registers);

/// @brief Extracts the processor type, family, model and stepping.
/// @param cpuinfo The struct to fill with the CPUID information.
/// @param registers The registers holding the result of the call.
/// @details CPUID is called with EAX=1. EAX then contains the type, family,
/// model and stepping ID; EBX contains the brand index, when the processor
/// supports one, and the APIC ID; ECX and EDX contain the feature bits, which
/// cpuid_feature_ecx and cpuid_feature_edx decode.
void cpuid_write_proctype(cpuinfo_t *cpuinfo, pt_regs_t *registers);

/// @brief Decodes the feature bits CPUID returns in ECX.
/// @param cpuinfo The struct whose cpuid_ecx_flags array is filled.
/// @param ecx The value of ECX after a CPUID call with EAX=1.
void cpuid_feature_ecx(cpuinfo_t *cpuinfo, uint32_t ecx);

/// @brief Decodes the feature bits CPUID returns in EDX.
/// @param cpuinfo The struct whose cpuid_edx_flags array is filled.
/// @param edx The value of EDX after a CPUID call with EAX=1.
void cpuid_feature_edx(cpuinfo_t *cpuinfo, uint32_t edx);

/// @brief Replaces one byte of a register value.
/// @param reg The register value to work on.
/// @param position The byte to replace, 0 being the least significant.
/// @param value The byte to put there; only its low eight bits are used.
/// @return The register value with that byte replaced.
uint32_t cpuid_get_byte(uint32_t reg, uint32_t position, uint32_t value);

/// @brief Resolves the brand index into a processor name.
/// @param f Stack frame holding the result of the CPUID call.
/// @return The brand string the index names, or NULL when the index is not
///         one this table knows.
/// @details Processors too old to report a full brand string report a single
/// byte in EBX instead, which indexes a fixed table of names published by the
/// vendor. cpuid_brand_string is the modern path and is preferred when
/// cpuinfo_t::is_brand_string says it is available.
char *cpuid_brand_index(pt_regs_t *f);

/// @brief Brand string is contained in EAX, EBX, ECX and EDX.
/// @param f Stack frame.
/// @return The brand string.
char *cpuid_brand_string(pt_regs_t *f);
