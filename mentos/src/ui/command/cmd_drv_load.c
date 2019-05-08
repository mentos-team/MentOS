///                MentOS, The Mentoring Operating system project
/// @file cmd_drv_load.c
/// @brief  
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "commands.h"
#include "mouse.h"
#include "string.h"
#include "stdio.h"

void cmd_drv_load(int argc, char **argv)
{
    if (argc < 2)
    {
        printf(
                "No driver inserted or bad usage! Type %s --help for the usage.\n",
                argv[0]);
    }
    else
    {
        if ((_kstrncmp(argv[1], "-r", 2) == 0))
        {
            if ((argv[2] != NULL))
            {
                if (_kstrncmp(argv[2], "mouse", 5) == 0)
                {
                    printf("Disattivamento %s in corso..\n", argv[2]);
                    mouse_disable();
                }
                else
                    printf("FATAL: Driver %s not found.\n", argv[2]);
            }
            else
                printf("Warning, no driver name inserted!\n");
        }
        else if (_kstrncmp(argv[1], "mouse", 5) == 0)
        {
            // Enabling mouse.
            mouse_install();
        }
        else if ((_kstrncmp(argv[1], "--help", 6) == 0) ||
                 (_kstrncmp(argv[1], "-h", 2) == 0))
        {
            printf("---------------------------------------------------\n"
                   "Driver tool to load and kill driver\n"
                   "Simple to use, just type:\n"
                   "\n"
                   "Usage: %s -<options> driver_name\n"
                   "\t-> %s module_name     - to load driver\n"
                   "\t-> %s -r module_name  - to kill driver\n"
                   "---------------------------------------------------\n",
                   argv[0], argv[0], argv[0]);
        }
        else
        {
            if ((_kstrncmp(argv[1], "-r", 2) == 0) &&
                (_kstrncmp(argv[2], "mouse", 5) == -1))
            {
                printf("FATAL: Driver %s not found.\n", argv[2]);
            }

            else
            {
                printf("FATAL: Driver %s not found.\n", argv[1]);
            }
        }
    }
}
