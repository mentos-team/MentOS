/// @file trace.h
/// @brief Optional diagnostics for IPC object lifetime and wait transitions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "io/debug.h"

/// @brief Emit an IPC diagnostic when explicitly enabled at configure time.
///
/// IPC tracing is intentionally opt-in: the normal kernel build does not add
/// logging or change any synchronization path.  Notice level is used so that
/// a trace-enabled build remains visible with the IPC source files' default
/// log-level configuration.
#ifdef ENABLE_IPC_TRACE
#define IPC_TRACE(...) pr_notice(__VA_ARGS__)
#else
#define IPC_TRACE(...) \
    do {               \
    } while (0)
#endif
