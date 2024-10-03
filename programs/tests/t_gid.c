/// @file t_gid.c
/// @brief Test group-related functions.
/// @details This program tests various group-related functions such as listing all groups,
/// resetting the group list, and retrieving group information by group ID and name.
/// It prints the details of each group and verifies the correctness of the `getgrgid` and `getgrnam` functions.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strerror.h>
#include <sys/unistd.h>

/// @brief List all groups and their members.
static void list_groups(void)
{
    struct group *iter;
    while ((iter = getgrent()) != NULL) {
        printf("Group name: \"%12s\", passwd: \"%12s\"\n", iter->gr_name, iter->gr_passwd);
        puts("Names: [ ");
        size_t count = 0;
        while (iter->gr_mem[count] != NULL) {
            printf("%s ", iter->gr_mem[count]);
            count += 1;
        }
        puts("]\n\n");
    }
}

int main(int argc, char **argv)
{
    printf("List of all groups:\n");

    // List all groups.
    list_groups();
    // Reset the group list to the beginning.
    setgrent();

    printf("List all groups again:\n");

    // List all groups again.
    list_groups();
    // Close the group list.
    endgrent();

    // Get the group information for the group with GID 0.
    struct group *root_group = getgrgid(0);
    if (root_group == NULL) {
        fprintf(STDERR_FILENO, "Error in getgrgid function: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (strcmp(root_group->gr_name, "root") != 0) {
        fprintf(STDERR_FILENO, "Error: Expected group name 'root', got '%s'\n", root_group->gr_name);
        return EXIT_FAILURE;
    }

    // Get the group information for the group named "root".
    root_group = getgrnam("root");
    if (root_group == NULL) {
        fprintf(STDERR_FILENO, "Error in getgrnam function: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if (root_group->gr_gid != 0) {
        fprintf(STDERR_FILENO, "Error: Expected GID 0, got %d\n", root_group->gr_gid);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
