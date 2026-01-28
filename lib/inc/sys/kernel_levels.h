/// @file   kernel_levels.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

// Integer equivalents of KERN_<LEVEL>
#define LOGLEVEL_DEFAULT (-1) ///< default-level messages.
#define LOGLEVEL_EMERG   0    ///< system is unusable.
#define LOGLEVEL_ALERT   1    ///< action must be taken immediately.
#define LOGLEVEL_CRIT    2    ///< critical conditions.
#define LOGLEVEL_ERR     3    ///< error conditions.
#define LOGLEVEL_WARNING 4    ///< warning conditions.
#define LOGLEVEL_NOTICE  5    ///< normal but significant condition.
#define LOGLEVEL_INFO    6    ///< informational.
#define LOGLEVEL_DEBUG   7    ///< debug-level messages.
