///                MentOS, The Mentoring Operating system project
/// @file cmd_mkdir.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stat.h"
#include "stdio.h"
#include "string.h"

void cmd_mkdir(int argc, char **argv)
{
	// Check the number of arguments.
	if (argc != 2) {
		printf("%s: missing operand.\n", argv[0]);
		printf("Try '%s --help' for more information.\n\n", argv[0]);

		return;
	}
	if (strcmp(argv[1], "--help") == 0) {
		printf("Creates a new directory.\n");
		printf("Usage:\n");
		printf("    %s <directory>\n", argv[0]);

		return;
	}
	mkdir(argv[1], 0);
}
