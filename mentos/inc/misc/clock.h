///                MentOS, The Mentoring Operating system project
/// @file clock.h
/// @brief Clock functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

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

/// Value used to retrieve the seconds.
#define SECOND_RTC 0x00

/// Value used to retrieve the minutes.
#define MINUTE_RTC 0x02

/// Value used to retrieve the hours.
#define HOUR_RTC 0x04

/// Value used to retrieve the day of the week.
#define DAY_W_RTC 0x06

/// Value used to retrieve the day of the month.
#define DAY_M_RTC 0x07

/// Value used to retrieve the month.
#define MONTH_RTC 0x08

/// Value used to retrieve the year.
#define YEAR_RTC 0x09

/// @brief Returns the milliseconds.
time_t get_millisecond();

/// @brief Returns the seconds.
time_t get_second();

/// @brief Returns the hours.
time_t get_hour();

/// @brief Returns the minutes.
time_t get_minute();

/// @brief Returns the day of the month.
time_t get_day_m();

/// @brief returns the month.
time_t get_month();

/// @brief Returns the year.
time_t get_year();

/// @brief Returns the day of the week.
time_t get_day_w();

/// @brief Returns the name of the month.
char *get_month_lng();

/// @brief Returns the name of the day.
char *get_day_lng();

/// @brief Returns the current time.
time_t time(time_t *timer);

// TODO: doxygen comment.
time_t difftime(time_t time1, time_t time2);

// TODO: doxygen comment.
void strhourminutesecond(char *dst);

// TODO: doxygen comment.
void strdaymonthyear(char *dst);

// TODO: doxygen comment.
void strdatehour(char *dst);
