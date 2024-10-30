/// @file id.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc == 1) {
        uid_t uid = geteuid();
        passwd_t *user = getpwuid(uid);
        gid_t gid = getegid();
        group_t *grp = getgrgid(gid);

        printf("uid=%d(%s) gid=%d(%s)\n", uid, user->pw_name, gid, grp->gr_name);
    } else if (strncmp(argv[1], "--help", 6) == 0) {
        printf("Usage: %s [OPTION]\n", argv[0]);
        printf("Print user and group information\n");
        printf("  -g, --group  print only the effective group ID\n");
        printf("  -u, --user   print only the effective user ID\n");
        printf("      --help   display this help and exit\n");
    } else if (strncmp(argv[1], "-u", 2) == 0 || strncmp(argv[1], "--user", 6) == 0) {
        printf("%d\n", geteuid());
    } else if (strncmp(argv[1], "-g", 2) == 0 || strncmp(argv[1], "--group", 7) == 0) {
        printf("%d\n", getegid());
    } else {
        printf("%s: invalid option '%s'\n", argv[0], argv[1]);
        printf("Try '%s --help' for more information\n", argv[0]);
    }

    return 0;
}
