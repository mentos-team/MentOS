///                MentOS, The Mentoring Operating system project
/// @file uname.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "stdio.h"
#include "video.h"
#include "string.h"
#include "utsname.h"
#include "version.h"
#include "cmd_cpuid.h"

void cmd_uname(int argc, char **argv)
{
	utsname_t utsname;
	uname(&utsname);
	if (argc != 2) {
		printf("%s\n", utsname.sysname);

		return;
	}

	if (!(strcmp(argv[1], "-a")) || !(strcmp(argv[1], "--all"))) {
		printf("%s %s #1 CEST 2013 %s\n", utsname.sysname, utsname.version,
			   sinfo.cpu_vendor);
	} else if (!(strcmp(argv[1], "-r")) || !(strcmp(argv[1], "--rev"))) {
		printf("%s\n", utsname.version);
	} else if (!(strcmp(argv[1], "-h")) || !(strcmp(argv[1], "--help"))) {
		printf(
			"Uname function allow you to see the kernel and system information.\n");
		printf("Function avaibles:\n");
		printf("1) -a   - Kernel version and processor type\n"
			   "2) -r   - Only the kernel version\n"
			   "3) -i   - All info of system and kernel\n");
	} else if (!(strcmp(argv[1], "-i")) || !(strcmp(argv[1], "--info"))) {
		printf("\n:==========: :System info: :==========:\n\n");
		printf("Version: %s\n", OS_VERSION);
		printf("Major: %d\n", OS_MAJOR_VERSION);
		printf("Minor: %d\n", OS_MINOR_VERSION);
		printf("Micro: %d\n", OS_MICRO_VERSION);

		// CPU Info.
        printf("\nCPU:");
		video_set_color(BRIGHT_RED);
		video_move_cursor(61, video_get_line());
        printf(sinfo.cpu_vendor);
		video_set_color(WHITE);
		printf("\n");

		// Memory RAM Info.
		/*
         * printf("Memory RAM: ");
         * video_move_cursor(60, video_get_line());
         * printf(" %d Kb\n", get_memsize()/1024);
         *
         * // Memory free RAM Info
         * printf(LNG_FREERAM);
         * video_move_cursor(60, video_get_line());
         * printf(" %d Kb\n", get_numpages());
         *
         * printf("\n");
         * // Bitmap Info
         * printf("Number bitmap's elements: ");
         * video_move_cursor(60, video_get_line());
         * printf(" %d", get_bmpelements());
         * video_move_cursor(60, video_get_line());
         */

		// Mem_area Info.
		/*
         * printf("\nSize of mem_area: ");
         * video_move_cursor(60, video_get_line());
         * printf(" %d\n", sizeof(mem_area));
         */

		// Page Dir Info.
		/*
         * printf("Page Dir Entry n.0 is: ");
         * video_move_cursor(60, video_get_line());
         * printf(" %d\n", get_pagedir_entry(0));
         */

		// Page Table Info.
		/*
         * printf("Page Table Entry n.4 in Page dir 0 is: ");
         * video_move_cursor(60, video_get_line());
         * printf(" %d\n", get_pagetable_entry(0,4));
         */

		printf("\n:==========: :===========: :==========:\n\n");
	} else {
		printf("%s. For more info about this tool, please do 'uname --help'\n",
			   utsname.sysname);
	}
}
