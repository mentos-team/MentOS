///                MentOS, The Mentoring Operating system project
/// @file cmd_cd.c
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <process/process.h>
#include <libc/stdlib.h>
#include "commands.h"
#include "vfs.h"
#include "stdio.h"
#include "dirent.h"
#include "string.h"
#include "libgen.h"

void cmd_cd(int argc, char **argv)
{
    DIR *dirp = NULL;
    char path[MAX_PATH_LENGTH];
    memset(path, 0, MAX_PATH_LENGTH);

    char current_path[MAX_PATH_LENGTH];
    getcwd(current_path, MAX_PATH_LENGTH);

    if (argc <= 1)
    {
        strcpy(path, "/");
    }
    else if (argc > 2)
    {
        printf("%s: too many arguments\n\n", argv[0]);

        return;
    }
    else if (strncmp(argv[1], "..", 2) == 0)
    {
        if (strcmp(current_path, dirname(current_path)) == 0)
        {
            return;
        }
        strcpy(path, dirname(current_path));
    }
    else if (strncmp(argv[1], ".", 1) == 0)
    {
        return;
    }
    else
    {
        // Copy the current path.
        strcpy(path, current_path);
        // Get the absolute path.
        get_absolute_path(path);
        // If the current directory is not the root, add a '/'.
        if (strcmp(path, "/") != 0)
        {
            strncat(path, "/", 1);
        }
        // Concatenate the input dir.
        strncat(path, argv[1], strlen(argv[1]));
    }

    dirp = opendir(path);
    if (dirp == NULL)
    {
        printf("%s: no such file or directory: %s\n\n", argv[0], argv[1]);

        return;
    }
    chdir(path);
    closedir(dirp);
}
