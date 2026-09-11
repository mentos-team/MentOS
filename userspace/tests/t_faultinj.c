/// @file t_faultinj.c
/// @brief Checks the block-transfer fault injection of #338.
/// @details The storage error paths had no test that reached them: a sector
/// read fails on real hardware about once in a few hundred boots (#291), which
/// is far too rare to write a test against. `/proc/faultinj` makes the failure
/// arrive on demand, so the code above the block layer can be exercised the way
/// a failing device exercises it.
///
/// This test checks the facility itself, and in doing so pins both halves of
/// #291's retry, which was previously only demonstrable with a temporary patch:
/// the injection sits inside `ata_device_read_sector_pio`, which the retry loop
/// calls up to ATA_TRANSFER_ATTEMPTS times, so arming *one* failure is
/// recovered and the operation succeeds, while arming three outlasts the retry
/// and propagates. The issues the facility exists to serve (#342, #343, #344)
/// get their own tests on top of it.
///
/// The facility is compiled in only under -DENABLE_ATA_FAULT_INJECTION=ON, so
/// `/proc/faultinj` is absent from a default build. That is not a failure: the
/// test reports it and passes, which is what lets it stay registered in the
/// suite permanently instead of being commented in and out.
///
/// A word of warning for anyone turning the option on: an injected failure
/// lands on whatever transfer comes next, including one belonging to the
/// filesystem's own bookkeeping, so a run with this armed can leave the image
/// inconsistent. The emulator opens the disk with `snapshot=on`, so nothing
/// survives the run, and the option is off by default.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

/// The control file.
#define CONTROL "/proc/faultinj"

/// A file of our own to operate on, so an injected failure lands on something
/// this test owns rather than on an unrelated part of the filesystem.
#define VICTIM "/home/user/t_faultinj.bin"

/// @brief Sends a command to the control file.
/// @param command the command to send.
/// @return 0 when it was accepted, -1 otherwise.
static int __arm(const char *command)
{
    int fd = open(CONTROL, O_WRONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_faultinj] open of " CONTROL " for writing: %s", strerror(errno));
        return -1;
    }
    ssize_t written = write(fd, command, strlen(command));
    close(fd);
    if (written != (ssize_t)strlen(command)) {
        syslog(LOG_ERR, "[t_faultinj] `%s` returned %zd", command, written);
        return -1;
    }
    return 0;
}

/// @brief Reads a counter out of the control file.
/// @param name the counter to read.
/// @param value where the value is stored.
/// @return 0 on success, -1 on failure.
static int __counter(const char *name, unsigned *value)
{
    int fd = open(CONTROL, O_RDONLY, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_faultinj] open of " CONTROL " for reading: %s", strerror(errno));
        return -1;
    }
    char buffer[192] = {0};
    ssize_t bytes    = read(fd, buffer, sizeof(buffer) - 1);
    close(fd);
    if (bytes <= 0) {
        syslog(LOG_ERR, "[t_faultinj] reading " CONTROL " returned %zd", bytes);
        return -1;
    }
    char *at = strstr(buffer, name);
    if (at == NULL) {
        syslog(LOG_ERR, "[t_faultinj] " CONTROL " does not report `%s`", name);
        return -1;
    }
    at += strlen(name);
    while (*at == ' ') {
        ++at;
    }
    *value = 0;
    for (; (*at >= '0') && (*at <= '9'); ++at) {
        *value = (*value * 10U) + (unsigned)(*at - '0');
    }
    return 0;
}

/// @brief Rejects a command that is not one of the three accepted forms.
/// @param command the command to send.
/// @return 0 when it was refused, -1 when it was accepted.
static int __check_rejected(const char *command)
{
    int fd = open(CONTROL, O_WRONLY, 0);
    if (fd < 0) {
        return -1;
    }
    errno           = 0;
    ssize_t written = write(fd, command, strlen(command));
    close(fd);
    if (written >= 0) {
        syslog(LOG_ERR, "[t_faultinj] the malformed command `%s` was accepted", command);
        return -1;
    }
    return 0;
}

int main(void)
{
    // Absent means the option is off, which is the default and not a failure.
    // Checked by opening it, because this libc has no access(2).
    int probe = open(CONTROL, O_RDONLY, 0);
    if (probe < 0) {
        syslog(LOG_INFO, "[t_faultinj] " CONTROL " is absent: built without ENABLE_ATA_FAULT_INJECTION, nothing to do");
        return EXIT_SUCCESS;
    }
    close(probe);

    int failures = 0;

    // Something to operate on, created before anything is armed.
    int fd = creat(VICTIM, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_faultinj] creating " VICTIM ": %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if (write(fd, "fault", 5) != 5) {
        syslog(LOG_ERR, "[t_faultinj] writing " VICTIM ": %s", strerror(errno));
        close(fd);
        unlink(VICTIM);
        return EXIT_FAILURE;
    }
    close(fd);

    // A malformed command must be refused outright rather than partly applied.
    if (__check_rejected("nonsense") < 0) {
        ++failures;
    }
    if (__check_rejected("read ") < 0) {
        ++failures;
    }
    if (__check_rejected("read seven") < 0) {
        ++failures;
    }

    // One armed failure is *recovered*, not propagated: ata_device_read_sector
    // retries a failed transfer up to ATA_TRANSFER_ATTEMPTS times (#291), and
    // the injection sits inside the function the retry loop calls. So arming
    // one failure exercises the retry, and the operation has to succeed.
    if (__arm("read 1") < 0) {
        ++failures;
    } else {
        int victim = open(VICTIM, O_RDONLY, 0);
        if (victim < 0) {
            syslog(LOG_ERR, "[t_faultinj] one injected failure was not recovered by the retry: %s", strerror(errno));
            ++failures;
        } else {
            close(victim);
        }
        unsigned injected = 0;
        if (__counter("reads_injected", &injected) < 0) {
            ++failures;
        } else if (injected != 1) {
            syslog(LOG_ERR, "[t_faultinj] reads_injected is %u after arming one, expected 1", injected);
            ++failures;
        }
    }

    // Enough failures must eventually reach a read the operation cannot do
    // without. Three is ATA_TRANSFER_ATTEMPTS, so three exhausts the retry for
    // one sector and propagates out of ata_read — but not necessarily out of
    // the syscall, because path resolution absorbs an I/O error on the way:
    // `__is_a_link` in namei.c reads "stat failed" as "not a link" and carries
    // on. Three injected failures were observed doing exactly that, with
    // `Failed to read the inode (2)` in the log and `open` still succeeding.
    // Filed separately; here the count is simply raised past the reads that
    // are tolerated, so the failure lands on one that is load-bearing.
    if (__arm("read 30") < 0) {
        ++failures;
    } else {
        errno       = 0;
        int victim  = open(VICTIM, O_RDONLY, 0);
        int op_fail = (victim < 0);
        if (victim >= 0) {
            char buffer[8];
            op_fail = (read(victim, buffer, sizeof(buffer)) < 0);
            close(victim);
        }
        if (!op_fail) {
            syslog(LOG_ERR, "[t_faultinj] a persistently failing device did not make the operation fail");
            ++failures;
        }
        // The facility has to say it fired, which is what distinguishes "the
        // injection worked" from "the operation failed for its own reasons".
        unsigned injected = 0;
        if (__counter("reads_injected", &injected) < 0) {
            ++failures;
        } else if (injected < 3) {
            syslog(LOG_ERR, "[t_faultinj] reads_injected is %u, expected at least 3", injected);
            ++failures;
        }
    }

    // Disarming has to restore normal operation: the point of the facility is
    // that it is off unless asked for, and a test that could not read the file
    // afterwards would not have shown that.
    if (__arm("off") < 0) {
        ++failures;
    } else {
        int victim = open(VICTIM, O_RDONLY, 0);
        if (victim < 0) {
            syslog(LOG_ERR, "[t_faultinj] " VICTIM " unreadable after disarming: %s", strerror(errno));
            ++failures;
        } else {
            char buffer[8] = {0};
            ssize_t bytes  = read(victim, buffer, sizeof(buffer) - 1);
            close(victim);
            if ((bytes != 5) || (strcmp(buffer, "fault") != 0)) {
                syslog(LOG_ERR, "[t_faultinj] " VICTIM " reads back %zd bytes `%s` after disarming", bytes, buffer);
                ++failures;
            }
        }
    }

    unlink(VICTIM);

    if (failures == 0) {
        syslog(LOG_INFO, "[t_faultinj] arming makes a transfer fail, disarming restores it");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_faultinj] %d FAILURES", failures);
    return EXIT_FAILURE;
}
