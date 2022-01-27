/// @file   kernel_levels.h
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#define KERN_SOH       "\001" ///< ASCII Start Of Header.
#define KERN_SOH_ASCII '\001' ///< Character version of the SOH.

#define KERN_EMERG   KERN_SOH "0" ///< Emergency messages (precede a crash)
#define KERN_ALERT   KERN_SOH "1" ///< Error requiring immediate attention
#define KERN_CRIT    KERN_SOH "2" ///< Critical error (hardware or software)
#define KERN_ERR     KERN_SOH "3" ///< Error conditions (common in drivers)
#define KERN_WARNING KERN_SOH "4" ///< Warning conditions (could lead to errors)
#define KERN_NOTICE  KERN_SOH "5" ///< Not an error but a significant condition
#define KERN_INFO    KERN_SOH "6" ///< Informational message
#define KERN_DEBUG   KERN_SOH "7" ///< Used only for debug messages
#define KERN_DEFAULT ""           ///< Default kernel logging level

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
