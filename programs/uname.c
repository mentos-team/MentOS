/// @file uname.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <sys/utsname.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    utsname_t utsname;
    uname(&utsname);
    if (argc != 2) {
        printf("%s\n", utsname.sysname);
        return -1;
    }

    if (!(strcmp(argv[1], "-a")) || !(strcmp(argv[1], "--all"))||
        !(strcmp(argv[1], "-i")) || !(strcmp(argv[1], "--info")))
    {
        printf("%s %s \n", utsname.sysname, utsname.version);
    } else if (!(strcmp(argv[1], "-r")) || !(strcmp(argv[1], "--rev"))) {
        printf("%s\n", utsname.version);
    } else if (!(strcmp(argv[1], "-i")) || !(strcmp(argv[1], "--info"))) {

    } else if (!(strcmp(argv[1], "-h")) || !(strcmp(argv[1], "--help"))) {
        printf("Uname function allow you to see the kernel and system information.\n");
        printf("Function avaibles:\n");
        printf(
            "1) -a   - Kernel version and processor type\n"
            "2) -r   - Only the kernel version\n"
            "3) -i   - All info of system and kernel\n"
        );
    } else {
        printf(
            "%s. For more info about this tool, please do 'uname --help'\n", utsname.sysname
        );
    }
    return 0;
}
