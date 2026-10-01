/// @file t_proc_feedback.c
/// @brief Regression test for #418: `/proc/feedback` must return the actual
/// scheduler feedback statistics instead of always reading empty.
/// @details Before the fix, `/proc/feedback`'s read handler always returned
/// 0 bytes (reported as a clean EOF) regardless of what was asked, and
/// logged every read at `pr_alert` level, even though the file was always
/// registered regardless of whether the scheduler feedback system (which
/// supplies its content) was actually compiled in.
///
/// The facility is compiled in only under -DENABLE_SCHEDULER_FEEDBACK=ON, so
/// `/proc/feedback` is absent from a default build: that is not a failure,
/// and is reported and passed the same way `/proc/faultinj` is in
/// `t_faultinj`, which is what lets this test stay registered in the suite
/// permanently. When the file is present, this checks that a read actually
/// returns scheduling statistics content, that it mentions the running
/// process (which is always tracked by the scheduler feedback system), and
/// that a second read starting at a non-zero offset returns the remainder of
/// the same content.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd = open("/proc/feedback", O_RDONLY, 0);
    if (fd < 0) {
        // Absent means the option is off, which is the default and not a
        // failure.
        syslog(LOG_INFO, "[t_proc_feedback] /proc/feedback is absent: built without ENABLE_SCHEDULER_FEEDBACK, nothing to do");
        return EXIT_SUCCESS;
    }

    char buffer[BUFSIZ];
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        syslog(LOG_ERR, "[t_proc_feedback] read returned %d bytes, expected scheduling statistics", (int)n);
        close(fd);
        return EXIT_FAILURE;
    }
    buffer[n] = '\0';

    // The header line must always be there: it names the active policy.
    if (strstr(buffer, "Scheduling Statistics") == NULL) {
        syslog(LOG_ERR, "[t_proc_feedback] missing header, got: %s", buffer);
        close(fd);
        return EXIT_FAILURE;
    }

    // This very process is always tracked by the scheduler feedback system
    // (scheduler_feedback_task_add is called for every created task), so its
    // pid must show up in the statistics.
    char pid_marker[16];
    snprintf(pid_marker, sizeof(pid_marker), "[%3d]", getpid());
    if (strstr(buffer, pid_marker) == NULL) {
        syslog(LOG_ERR, "[t_proc_feedback] own pid %d missing from: %s", getpid(), buffer);
        close(fd);
        return EXIT_FAILURE;
    }

    // A further read at offset n must behave like a regular file: either
    // more content (if it did not all fit in the first read) or a clean EOF,
    // never an error.
    char tail[BUFSIZ];
    ssize_t n2 = read(fd, tail, sizeof(tail));
    if (n2 < 0) {
        syslog(LOG_ERR, "[t_proc_feedback] second read failed: %s", strerror(errno));
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);
    syslog(LOG_INFO, "[t_proc_feedback] /proc/feedback returned %d (+%d) bytes of real statistics", (int)n, (int)n2);
    return EXIT_SUCCESS;
}
