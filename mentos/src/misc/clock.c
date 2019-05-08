///                MentOS, The Mentoring Operating system project
/// @file clock.c
/// @brief Clock functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "clock.h"
#include "timer.h"
#include "stdio.h"
#include "stddef.h"
#include "port_io.h"

time_t get_millisecond()
{
	return timer_get_subticks();
}

time_t get_second()
{
	outportb(0x70, SECOND_RTC);
	time_t c = inportb(0x71);

	return c;
}

time_t get_minute()
{
	outportb(0x70, MINUTE_RTC);
	time_t c = inportb(0x71);

	return c;
}

time_t get_hour()
{
	outportb(0x70, HOUR_RTC);
	time_t c = inportb(0x71);

	return c;
}

time_t get_day_w()
{
	outportb(0x70, DAY_W_RTC);
	time_t c = inportb(0x71);

	return c;
}

time_t get_day_m()
{
	outportb(0x70, DAY_M_RTC);
	time_t c = inportb(0x71);

	return c;
}

time_t get_month()
{
	outportb(0x70, MONTH_RTC);
	time_t c = inportb(0x71);

	return c;
}

time_t get_year()
{
	outportb(0x70, YEAR_RTC);
	time_t c = inportb(0x71);

	return c;
}

char *get_month_lng()
{
	switch (get_month()) {
	case 1:
		return "January";
	case 2:
		return "February";
	case 3:
		return "March";
	case 4:
		return "April";
	case 5:
		return "May";
	case 6:
		return "June";
	case 7:
		return "July";
	case 8:
		return "August";
	case 9:
		return "September";
	case 10:
		return "October";
	case 11:
		return "November";
	case 12:
		return "December";
	default:
		break;
	}
	return "";
}

char *get_day_lng()
{
	switch (get_day_w()) {
	case 1:
		return "Sunday";
	case 2:
		return "Monday";
	case 3:
		return "Tuesday";
	case 4:
		return "Wednesday";
	case 5:
		return "Thursday";
	case 6:
		return "Friday";
	case 7:
		return "Saturday";
	default:
		break;
	}
	return "";
}

time_t time(time_t *timer)
{
	// Jan 1, 1970
	time_t t = 0;
	t += get_millisecond();
	t += get_second();
	t += get_minute() * 60;
	t += get_hour() * 3600;
	t += get_day_m() * 86400;
	t += get_month() * 2629743;
	t += (1970 - get_year()) * 31556926;
	if (timer != NULL) {
		(*timer) = t;
	}

	return t;
}

time_t difftime(time_t time1, time_t time2)
{
	return time1 - time2;
}

void strhourminutesecond(char *dst)
{
	time_t s, m, h;
	s = get_second();
	m = get_minute();
	h = get_hour();

	sprintf(dst, "%i:%i:%i", h, m, s);
}

void strdaymonthyear(char *dst)
{
	sprintf(dst, "%s %i %s %i", get_day_lng(), get_day_m(), get_month_lng(),
			1970 - get_year());
}

void strdatehour(char *dst)
{
	time_t s, m, h;
	s = get_second();
	m = get_minute();
	h = get_hour();

	sprintf(dst, "%s %i %s %i %i:%i:%i", get_day_lng(), get_day_m(),
			get_month_lng(), 1970 - get_year(), h, m, s);
}
