/// @file chmod.c
/// @brief change file permissions
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <strerror.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/unistd.h>

int main(int argc, char *argv[])
{
   mode_t mode = 0;
   char *endptr;
   gid_t gid = -1;

   if (argc != 3) {
       fprintf(STDERR_FILENO, "%s: MODE FILE\n", argv[0]);
       exit(EXIT_FAILURE);
   }

   mode = strtol(argv[1], &endptr, 8);
   if (*endptr != '\0') {
       fprintf(STDERR_FILENO, "%s: invalid mode: '%s'\n", argv[0], argv[1]);
       exit(EXIT_FAILURE);
   }

   if (chmod(argv[2], mode) == -1) {
       fprintf(STDERR_FILENO, "%s: changing permissions of %s: %s\n",
               argv[0], argv[2], strerror(errno));
       exit(EXIT_FAILURE);
   }

   exit(EXIT_SUCCESS);
}
