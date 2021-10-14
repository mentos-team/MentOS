///                MentOS, The Mentoring Operating system project
/// @file pwd.c
/// @brief
/// @copyright (c) 2014-2021 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/unistd.h>
#include <limits.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    printf("%s\n", cwd);
    return 0;
}
