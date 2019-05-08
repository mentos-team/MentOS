///                MentOS, The Mentoring Operating system project
/// @file cmd_ps.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "list.h"
#include "stdio.h"
#include "string.h"
#include "bitops.h"
#include "scheduler.h"

#define PS_OPT_F (1 << 1)

void cmd_ps(int argc, char **argv)
{
	// Flag variable.
	uint32_t flags = 0;

	// Check arguments.
	for (int i = 0; i < argc; ++i) {
		size_t optlen = strlen(argv[i]);
		if (optlen == 0) {
			continue;
		}

		if (argv[i][0] != '-') {
			continue;
		}

		for (size_t j = 1; j < optlen; ++j) {
			if (argv[i][j] == 'f') {
				set_flag(&flags, PS_OPT_F);
			}
		}
	}

	if (has_flag(flags, PS_OPT_F)) {
		// Print the header.
		printf("%-6s", "PID");
		printf("%-6s", "STATE");
		printf("%-50s", "COMMAND");
		printf("\n");
		// print_tree(task_structree->root, 0);
	} else {
		// Print the header.
		printf("%-6s", "PID");
		printf("%-6s", "STATE");
		printf("%-50s", "COMMAND");
		printf("\n");
	}
	printf("\n");
}
