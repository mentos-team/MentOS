/// @file termios.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "termios.h"
#include "system/syscall_types.h"
#include "sys/errno.h"
#include "bits/ioctls.h"

int tcgetattr(int fd, termios_t *termios_p)
{
    int retval;
    __asm__ volatile("int $0x80"
                     : "=a"(retval)
                     : "0"(__NR_ioctl), "b"((long)(fd)), "c"((long)(TCGETS)),
                       "d"((long)(termios_p)));
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}

int tcsetattr(int fd, int optional_actions, const termios_t *termios_p)
{
    int retval;
    __asm__ volatile("int $0x80"
                     : "=a"(retval)
                     : "0"(__NR_ioctl), "b"((long)(fd)), "c"((long)(TCSETS)),
                       "d"((long)(termios_p)));
    if (retval < 0) {
        errno = -retval;
        retval = -1;
    }
    return retval;
}
