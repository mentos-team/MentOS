///                MentOS, The Mentoring Operating system project
/// @file cmd_date.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "clock.h"
#include "irqflags.h"

void cmd_date(int argc, char **argv)
{
	(void)argc;
	(void)argv;
	printf("It's %x:%x:%x of %s %02x %s %02x\n", get_hour(),
		   get_minute(), get_second(), get_day_lng(),
		   get_day_m(), get_month_lng(), 0x2000 + get_year());
}
