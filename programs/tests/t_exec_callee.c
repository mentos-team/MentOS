/// @file t_exec_callee.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    char *ENV_VAR = getenv("ENV_VAR");
    if (ENV_VAR == NULL) {
        printf("Failed to get env: `ENV_VAR`\n");
        return 1;
    }
    printf("ENV_VAR = %s\n", ENV_VAR);
    return 0;
}
