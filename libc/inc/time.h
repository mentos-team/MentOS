/// @file time.h
/// @brief Time-related functions.
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stddef.h"

/// @brief This timer counts down in real (i.e., wall clock) time. At each
/// expiration, a SIGALRM signal is generated.
#define ITIMER_REAL 0
/// @brief This timer counts down against the user-mode CPU time consumed by the
/// process. At each expiration, a SIGVTALRM signal is generated.
#define ITIMER_VIRTUAL 1
/// @brief This timer counts down against the total (i.e., both user and system)
/// CPU time consumed by the process. At each expiration, a SIGPROF signal is
/// generated.
#define ITIMER_PROF 2

/// Used to store time values.
typedef unsigned int time_t;

/// Used to get information about the current time.
typedef struct tm_t {
    /// Seconds [0 to 59]
    int tm_sec;
    /// Minutes [0 to 59]
    int tm_min;
    /// Hours [0 to 23]
    int tm_hour;
    /// Day of the month [1 to 31]
    int tm_mday;
    /// Month [0 to 11]
    int tm_mon;
    /// Year [since 1900]
    int tm_year;
    /// Day of the week [0 to 6]
    int tm_wday;
    /// Day in the year [0 to 365]
    int tm_yday;
    /// Is Daylight Saving Time.
    int tm_isdst;
} tm_t;

/// Rappresents time.
typedef struct _timeval {
    time_t tv_sec;  ///< Seconds.
    time_t tv_usec; ///< Microseconds.
} timeval;

/// Rappresents a time interval.
typedef struct _itimerval {
    timeval it_interval; ///< Next value.
    timeval it_value;    ///< Current value.
} itimerval;

/// @brief Specify intervals of time with nanosecond precision.
typedef struct timespec {
    time_t tv_sec; ///< Seconds.
    long tv_nsec;  ///< Nanoseconds.
} timespec;

/// @brief Returns the current time.
/// @param t Where the time should be stored.
/// @return The current time.
time_t time(time_t *t);

/// @brief Return the difference between the two time values.
/// @param time1 The first time value.
/// @param time2 The second time value.
/// @return The difference in terms of seconds.
time_t difftime(time_t time1, time_t time2);

/// @brief The current time broken down into a tm_t structure.
/// @param timep A pointer to a variable holding the current time.
/// @return The time broken down.
tm_t *localtime(const time_t *timep);

/// @brief Formats the time tm according to the format specification format
///        and places the result in the character array s of size max.
/// @param s      The destination buffer.
/// @param max    The maximum length of the buffer.
/// @param format The buffer used to generate the time.
/// @param tm     The broken-down time.
/// @return The number of bytes (excluding the terminating null) placed in s.
size_t strftime(char *s, size_t max, const char *format, const tm_t *tm);

/// @brief Suspends the execution of the calling thread.
/// @param req The amount of time we want to sleep.
/// @param rem The remaining time we did not sleep.
/// @return If the call is interrupted by a signal handler, nanosleep()
///         returns -1, sets errno to EINTR, and writes the remaining time
///         into the structure pointed to by rem unless rem is NULL.
/// @details
/// The execution is suspended until either at least the time specified
/// in *req has elapsed, or the delivery of a signal that triggers the
/// invocation of a handler in the calling thread or that terminates
/// the process.
int nanosleep(const timespec *req, timespec *rem);

/// @brief Causes the calling thread to sleep either until the number of
///        real-time seconds specified in seconds have elapsed or
///        until a signal arrives which is not ignored.
/// @param seconds The number of seconds we want to sleep.
/// @return Zero if the requested time has elapsed, or the number of seconds
///         left to sleep, if the call was interrupted by a signal handler.
unsigned int sleep(unsigned int seconds);

/// @brief Fills the structure pointed to by curr_value with the current setting
/// for the timer specified by which.
/// @param which which timer.
/// @param curr_value where we place the timer value.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int getitimer(int which, itimerval *curr_value);

/// @brief The system provides each process with three interval timers, each
/// decrementing in a distinct time domain. When any timer expires, a signal is
/// sent to the process, and the timer (potentially) restarts.
/// @param which which timer.
/// @param new_value the new value for the timer.
/// @param old_value output variable where the function places the previous value.
/// @return 0 on success, -1 on failure and errno is set to indicate the error.
int setitimer(int which, const itimerval *new_value, itimerval *old_value);
