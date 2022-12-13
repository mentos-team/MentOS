/// @file t_gid.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "grp.h"
#include "stdlib.h"
#include "string.h"
#include <stdio.h>

static void list_groups()
{
    group_t *iter;
    while ((iter = getgrent()) != NULL) {
        printf("Group\n\tname: %s\n\tpasswd: %s\n\tnames:\n", iter->gr_name, iter->gr_passwd);

        size_t count = 0;
        while (iter->gr_mem[count] != NULL) {
            printf("\t\t%s\n", iter->gr_mem[count]);
            count += 1;
        }

        printf("\n");
    }
}

int main(int argc, char **argv)
{
    printf("List of all groups:\n");

    list_groups();
    setgrent();

    printf("List all groups again:\n");

    list_groups();
    endgrent();

    group_t *root_group = getgrgid(0);
    if (strcmp(root_group->gr_name, "root") != 0) {
        printf("Error in getgrgid function.");
        return 1;
    }

    root_group = getgrnam("root");
    if (root_group->gr_gid != 0) {
        printf("Error in getgrnam function.");
        return 1;
    }

    return 0;
}
