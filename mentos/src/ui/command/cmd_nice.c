///                MentOS, The Mentoring Operating system project
/// @file cmd_nice.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "unistd.h"
#include "string.h"

void cmd_nice(int argc, char **argv)
{
	if (argc == 1) {
		int current = nice(0);
		printf("%d \n\n", current);

		return;
	}

	if (argc != 2) {
		printf("%s: missing operand.\n", argv[0]);
		printf("Try '%s --help' for more information.\n\n", argv[0]);

		return;
	}

	if (!strcmp(argv[1], "--help")) {
		printf("Usage: %s <increment>\n\n", argv[0]);

		return;
	}

	int increment = atoi(argv[1]);
	if ((increment < -40) || (increment > 40)) {
		printf("Error: You must provide a value between (-40,+40). \n\n",
			   increment);

		return;
	}
	int newNice = nice(increment);
	printf("Your new nice value is %d.\n\n", newNice);
}
