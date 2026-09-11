/// @file pwd.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <strerror.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    char cwd[PATH_MAX];
    if (getcwd(cwd, PATH_MAX) == NULL) {
        fprintf(stderr, "%s: cannot get current working directory: %s\n", argv[0], strerror(errno));
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}
