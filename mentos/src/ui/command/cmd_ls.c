///                MentOS, The Mentoring Operating system project
/// @file ls.c
/// @brief Command 'ls'.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <process/process.h>
#include <libc/stdlib.h>
#include "commands.h"
#include "vfs.h"
#include "stdio.h"
#include "video.h"
#include "dirent.h"
#include "string.h"
#include "bitops.h"
#include "libgen.h"
#include "strerror.h"

#define FLAG_L 1

static void print_ls(DIR *dirp, uint32_t flags)
{
	// If the directory is open.
	if (dirp == NULL) {
		return;
	}

	size_t total_size = 0;
	dirent_t *dirent = readdir(dirp);
	while (dirent != NULL) {
		if (dirent->d_type == FS_DIRECTORY) {
			video_set_color(BRIGHT_CYAN);
		}
		if (dirent->d_type == FS_MOUNTPOINT) {
			video_set_color(BRIGHT_GREEN);
		}
		if (has_flag(flags, FLAG_L)) {
			stat_t entry_stat;
			if (stat(dirent->d_name, &entry_stat) != -1) {
				printf("%d %3d %3d %8d %s\n", dirent->d_type, entry_stat.st_uid,
					   entry_stat.st_gid, entry_stat.st_size,
					   basename(dirent->d_name));
				total_size += entry_stat.st_size;
			}
		} else {
			printf("%s ", basename(dirent->d_name));
		}
		video_set_color(WHITE);
		dirent = readdir(dirp);
	}

	closedir(dirp);
	printf("\n");
	if (has_flag(flags, FLAG_L)) {
		printf("Total: %d byte\n", total_size);
	}
	printf("\n");
}

void cmd_ls(int argc, char **argv)
{
	// Create a variable to store flags.
	uint32_t flags = 0;
	// Check the number of arguments.
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			printf("List information about files inside a given directory.\n");
			printf("Usage:\n");
			printf("    ls [options] [directory]\n\n");

			return;
		} else if (strcmp(argv[i], "-l") == 0) {
			set_flag(&flags, FLAG_L);
		}
	}

	bool_t no_directory = true;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-l") == 0) {
			continue;
		}

		no_directory = false;
		DIR *dirp = opendir(argv[i]);
		if (dirp == NULL) {
			printf("%s: cannot access '%s': %s\n\n", argv[0], argv[i],
				   "unknown" /*strerror(errno)*/);

			continue;
		}
		printf("%s:\n", argv[i]);
		print_ls(dirp, flags);
	}
	if (no_directory) {
		char cwd[MAX_PATH_LENGTH];
		getcwd(cwd, MAX_PATH_LENGTH);
		DIR *dirp = opendir(cwd);
		if (dirp == NULL) {
			printf("%s: cannot access '%s': %s\n\n", argv[0], cwd, "unknown");
		} else {
			print_ls(dirp, flags);
		}
	}
}
