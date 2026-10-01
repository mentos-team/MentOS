/// @file t_nice.c
/// @brief Regression test for #409: negative nice values are valid results.
/// @copyright (c) 2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

int main(void)
{
    errno         = 0;
    int nice_value = nice(-1);
    if (nice_value != -1 || errno != 0) {
        syslog(LOG_ERR, "[t_nice] nice(-1) returned %d with errno %d, expected -1 with errno 0", nice_value, errno);
        return EXIT_FAILURE;
    }

    nice_value = nice(1);
    if (nice_value != 0) {
        syslog(LOG_ERR, "[t_nice] nice(1) returned %d, expected 0", nice_value);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
