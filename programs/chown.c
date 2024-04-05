/// @file chown.c
/// @brief change file ownership
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <grp.h>
#include <pwd.h>
#include <strerror.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/unistd.h>

int main(int argc, char *argv[])
{
    char *idptr;
    char *saveptr;
    char *endptr;
    uid_t uid = -1;
    gid_t gid = -1;
    passwd_t *pwd;
    group_t *grp;

    if (argc != 3) {
        fprintf(STDERR_FILENO, "%s: [OWNER][:[GROUP]] FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    idptr = strtok_r(argv[1], ":", &saveptr);
    if (argv[1][0] != ':') {               /* Skip the username */
        uid = strtol(idptr, &endptr, 10);  /* Allow a numeric string */
        if (*endptr != '\0') {             /* Was not pure numeric string */
            pwd = getpwnam(idptr);         /* Try getting UID for username */
            if (pwd == NULL) {
                fprintf(STDERR_FILENO, "%s: invalid user: %s\n", argv[0], idptr);
                exit(EXIT_FAILURE);
            }

            uid = pwd->pw_uid;
        }
        idptr = strtok_r(NULL, ":", &saveptr);
    }

    if (idptr != NULL) {
        gid = strtol(idptr, &endptr, 10);  /* Allow a numeric string */
        if (*endptr != '\0') {             /* Was not pure numeric string */
            grp = getgrnam(idptr);         /* Try getting GID for groupname */
            if (grp == NULL) {
                fprintf(STDERR_FILENO, "%s: invalid group: %s\n", argv[0], idptr);
                exit(EXIT_FAILURE);
            }

            gid = grp->gr_gid;
        }
    }

    if (chown(argv[2], uid, gid) == -1) {
        fprintf(STDERR_FILENO, "%s: changing ownership of %s: %s\n",
                argv[0], argv[2], strerror(errno));
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}
