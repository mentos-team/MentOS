/// @file env.c
/// @brief `env` program.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <unistd.h>

extern char **environ;

int main(int argc, char *argv[])
{
    for (char **ep = environ; *ep; ++ep)
        printf("%s\n", *ep);
    return 0;
}
