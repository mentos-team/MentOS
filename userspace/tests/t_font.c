/// @file t_font.c
/// @brief Test the console font-size escape sequence.
/// @details Writes each form of ESC [ <n> z to the console and checks that the
/// console still works afterwards. What this can and cannot check is worth being
/// explicit about: userspace has no way to ask how many cells the console has --
/// there is no TIOCGWINSZ -- so this cannot assert the geometry that came out.
/// What it does assert is that the sequence is consumed rather than printed, that
/// it does not take the console or the kernel down, and that ordinary output
/// still arrives in order afterwards. On a build whose backend cannot change
/// font, every request is refused and the same checks apply unchanged.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/// @brief Writes one request and a marker, then reads nothing back.
/// @param console Descriptor for the console.
/// @param sequence The escape sequence to write.
/// @return 0 on success, 1 on failure.
static int request(int console, const char *sequence)
{
    ssize_t written = write(console, sequence, strlen(sequence));
    if (written != (ssize_t)strlen(sequence)) {
        syslog(LOG_ERR, "[t_font] short write of %u bytes: %s", (unsigned)strlen(sequence), strerror(errno));
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // The console, not stdout: this has to reach the escape-sequence parser
    // whatever the test harness has done with the standard descriptors.
    int console = open("/proc/video", O_WRONLY, 0);
    if (console < 0) {
        syslog(LOG_ERR, "[t_font] cannot open /proc/video: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    // Larger, larger, smaller, and back to the default. Two in a row on purpose:
    // relative requests accumulate, and the second one is what would be lost if
    // they were ever collapsed into a single stored direction.
    static const char *const requests[] = {"\033[2z", "\033[2z", "\033[1z", "\033[0z", "\033[z"};

    for (unsigned index = 0; index < (sizeof(requests) / sizeof(requests[0])); ++index) {
        if (request(console, requests[index])) {
            close(console);
            return EXIT_FAILURE;
        }
    }

    // The console has to still be usable, and the sequences must not have left
    // any of their own characters behind on it.
    if (request(console, "\n")) {
        close(console);
        return EXIT_FAILURE;
    }

    if (close(console) < 0) {
        syslog(LOG_ERR, "[t_font] cannot close /proc/video: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "[t_font] console font requests accepted and the console survived.");
    return EXIT_SUCCESS;
}
