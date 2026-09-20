/// @file t_userptr.c
/// @brief Regression test for #191: every pointer-taking syscall must
/// refuse a pointer that does not name the caller's memory.
/// @details There is no user-pointer validation layer: syscall handlers
/// dereference raw caller pointers with supervisor rights, so `read`
/// wrote wherever its buffer pointed — including kernel memory — and a
/// pointer to unmapped memory took the whole kernel down with a page
/// fault inside the handler. This test pins the first four gated
/// syscalls (`read`, `write`, `time`, `pipe`): each must answer `-EFAULT`
/// for a kernel pointer, an address straddling the kernel boundary, the
/// identity-mapped supervisor-only first megabyte every address space
/// inherits, an unmapped user address, and NULL, while the legitimate
/// uses keep working and the kernel survives it all.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <syslog.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "system/syscall_types.h"

/// Start of the kernel area: above this, nothing belongs to a task.
#define KERNEL_TOP ((void *)0xC0000000UL)

/// The video buffer inside the identity-mapped first megabyte: present in
/// every page directory, but supervisor-only, so the hardware never
/// protects the kernel from a syscall reaching for it.
#define VGA_MEMORY ((void *)0x000B8000UL)

/// An address nothing maps: above the first megabyte, below the lowest
/// mapped user segment.
#define UNMAPPED_USER ((void *)0x00200000UL)

/// @brief Expects a call to fail with EFAULT.
/// @param what the description of the call, for the failure message.
/// @param expression the call, evaluating to its return value.
/// @return 0 when the call failed with EFAULT, -1 otherwise.
#define EXPECT_EFAULT(what, expression)                                                        \
    do {                                                                                       \
        errno = 0;                                                                             \
        if ((expression) != -1) {                                                              \
            syslog(LOG_ERR, "[t_userptr] %s succeeded, expected EFAULT", what);                \
            return -1;                                                                         \
        }                                                                                      \
        if (errno != EFAULT) {                                                                 \
            syslog(LOG_ERR, "[t_userptr] %s: expected EFAULT, got %s", what, strerror(errno)); \
            return -1;                                                                         \
        }                                                                                      \
    } while (0)

/// @brief A bad read buffer must never reach the filesystem layer.
/// @param fd the descriptor to read from.
/// @return 0 on success, -1 on failure.
static int check_read_pointers(int fd)
{
    char buffer[8] = {0};
    EXPECT_EFAULT("read into the kernel area", read(fd, KERNEL_TOP, 4));
    EXPECT_EFAULT("read across the kernel boundary", read(fd, (char *)KERNEL_TOP - 2, 4));
    EXPECT_EFAULT("read into the supervisor-only first megabyte", read(fd, VGA_MEMORY, 4));
    EXPECT_EFAULT("read into unmapped memory", read(fd, UNMAPPED_USER, 4));
    EXPECT_EFAULT("read into NULL", read(fd, NULL, 4));
    // The legitimate use must keep working.
    if (read(fd, buffer, sizeof(buffer)) < 0) {
        syslog(LOG_ERR, "[t_userptr] read into a real buffer: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief A bad write buffer must never reach the filesystem layer.
/// @param fd the descriptor to write to.
/// @return 0 on success, -1 on failure.
static int check_write_pointers(int fd)
{
    const char text[] = "USERPTR";
    EXPECT_EFAULT("write from the kernel area", write(fd, KERNEL_TOP, 4));
    EXPECT_EFAULT("write from the supervisor-only first megabyte", write(fd, VGA_MEMORY, 4));
    EXPECT_EFAULT("write from unmapped memory", write(fd, UNMAPPED_USER, 4));
    // The legitimate use must keep working, and be readable back.
    if (write(fd, text, sizeof(text)) != (ssize_t)sizeof(text)) {
        syslog(LOG_ERR, "[t_userptr] write from a real buffer: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief The scalar outputs of time and the descriptor pair of pipe are
///        stored through caller pointers too.
/// @return 0 on success, -1 on failure.
static int check_scalar_outputs(void)
{
    time_t now = 0;
    EXPECT_EFAULT("time into the kernel area", time((time_t *)KERNEL_TOP));
    // time(NULL) is a legitimate request for the value alone.
    if (time(NULL) == (time_t)-1) {
        syslog(LOG_ERR, "[t_userptr] time(NULL): %s", strerror(errno));
        return -1;
    }
    if (time(&now) == (time_t)-1) {
        syslog(LOG_ERR, "[t_userptr] time into a real pointer: %s", strerror(errno));
        return -1;
    }
    int fds[2];
    EXPECT_EFAULT("pipe into the kernel area", pipe((int *)KERNEL_TOP));
    EXPECT_EFAULT("pipe into unmapped memory", pipe((int *)UNMAPPED_USER));
    EXPECT_EFAULT("pipe into NULL", pipe(NULL));
    if (pipe(fds) < 0) {
        syslog(LOG_ERR, "[t_userptr] pipe into a real array: %s", strerror(errno));
        return -1;
    }
    close(fds[0]);
    close(fds[1]);
    return 0;
}

/// @brief Expects a pointer-returning call to fail with EFAULT.
#define EXPECT_EFAULT_PTR(what, expression)                                                    \
    do {                                                                                       \
        errno = 0;                                                                             \
        if ((expression) != (void *)-1) {                                                      \
            syslog(LOG_ERR, "[t_userptr] %s succeeded, expected EFAULT", what);                \
            return -1;                                                                         \
        }                                                                                      \
        if (errno != EFAULT) {                                                                 \
            syslog(LOG_ERR, "[t_userptr] %s: expected EFAULT, got %s", what, strerror(errno)); \
            return -1;                                                                         \
        }                                                                                      \
    } while (0)

/// @brief Expects a NULL-on-error pointer call to fail with EFAULT.
#define EXPECT_EFAULT_NULL(what, expression)                                                   \
    do {                                                                                       \
        errno = 0;                                                                             \
        if ((expression) != NULL) {                                                            \
            syslog(LOG_ERR, "[t_userptr] %s succeeded, expected EFAULT", what);                \
            return -1;                                                                         \
        }                                                                                      \
        if (errno != EFAULT) {                                                                 \
            syslog(LOG_ERR, "[t_userptr] %s: expected EFAULT, got %s", what, strerror(errno)); \
            return -1;                                                                         \
        }                                                                                      \
    } while (0)

/// @brief Calls the waitpid syscall without the libc wrapper, which
///        dereferences the status pointer itself on return.
static pid_t raw_waitpid(pid_t pid, int *status, int options)
{
    long __res;
    __inline_syscall_3(__res, waitpid, pid, status, options);
    if (__res < 0) {
        errno = (int)-__res;
        return -1;
    }
    return (pid_t)__res;
}

/// @brief Calls the semop syscall directly: the libc wrapper issues one
///        syscall per array element, which would walk the probe addresses
///        itself.
static long raw_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    long __res;
    __inline_syscall_3(__res, semop, semid, sops, nsops);
    if (__res < 0) {
        errno = (int)-__res;
        return -1;
    }
    return __res;
}

/// @brief The fixed-size in/out pointers of the process and signal
///        families (#191 stage two).
/// @return 0 on success, -1 on failure.
static int check_scalar_pointers(void)
{
    stat_t st;
    struct itimerval itv = {0};
    struct timespec ts   = {0};
    sched_param_t sparam;
    sighandler_t old = SIG_ERR;
    EXPECT_EFAULT("waitpid into the kernel area", raw_waitpid(-1, (int *)KERNEL_TOP, WNOHANG));
    EXPECT_EFAULT("waitpid into unmapped memory", raw_waitpid(-1, (int *)UNMAPPED_USER, WNOHANG));
    EXPECT_EFAULT("stat into the kernel area", stat("/home/user", (stat_t *)KERNEL_TOP));
    EXPECT_EFAULT("stat of a kernel-area path", stat((const char *)KERNEL_TOP, &st));
    EXPECT_EFAULT("stat of an unmapped path", stat((const char *)UNMAPPED_USER, &st));
    EXPECT_EFAULT("stat of NULL", stat(NULL, &st));
    EXPECT_EFAULT("fstat into the kernel area", fstat(0, (stat_t *)KERNEL_TOP));
    EXPECT_EFAULT("fstat across the kernel boundary", fstat(0, (stat_t *)((char *)KERNEL_TOP - 2)));
    EXPECT_EFAULT("statfs into the kernel area", statfs("/home/user", (statfs_t *)KERNEL_TOP));
    EXPECT_EFAULT("fstatfs into the kernel area", fstatfs(0, (statfs_t *)KERNEL_TOP));
    EXPECT_EFAULT("uname into the kernel area", uname((utsname_t *)KERNEL_TOP));
    EXPECT_EFAULT("uname into unmapped memory", uname((utsname_t *)UNMAPPED_USER));
    EXPECT_EFAULT("uname into NULL", uname(NULL));
    EXPECT_EFAULT("sigaction from the kernel area", sigaction(SIGUSR1, (const struct sigaction *)KERNEL_TOP, NULL));
    EXPECT_EFAULT("sigaction into the kernel area", sigaction(SIGUSR1, NULL, (struct sigaction *)KERNEL_TOP));
    EXPECT_EFAULT("sigprocmask from the kernel area", sigprocmask(SIG_BLOCK, (const sigset_t *)KERNEL_TOP, NULL));
    EXPECT_EFAULT("sigprocmask into the kernel area", sigprocmask(SIG_BLOCK, NULL, (sigset_t *)KERNEL_TOP));
    EXPECT_EFAULT("nanosleep from the kernel area", nanosleep((const struct timespec *)KERNEL_TOP, NULL));
    EXPECT_EFAULT("nanosleep into the kernel area", nanosleep(&ts, (struct timespec *)KERNEL_TOP));
    EXPECT_EFAULT("getitimer into the kernel area", getitimer(ITIMER_REAL, (struct itimerval *)KERNEL_TOP));
    EXPECT_EFAULT("setitimer from the kernel area", setitimer(ITIMER_REAL, (const struct itimerval *)KERNEL_TOP, NULL));
    EXPECT_EFAULT("setitimer into the kernel area", setitimer(ITIMER_REAL, &itv, (struct itimerval *)KERNEL_TOP));
    EXPECT_EFAULT("sched_getparam into the kernel area", sched_getparam(0, (sched_param_t *)KERNEL_TOP));
    EXPECT_EFAULT("sched_setparam from the kernel area", sched_setparam(0, (const sched_param_t *)KERNEL_TOP));
    EXPECT_EFAULT_NULL("getcwd into the kernel area", getcwd((char *)KERNEL_TOP, 128));
    EXPECT_EFAULT_NULL("getcwd into unmapped memory", getcwd((char *)UNMAPPED_USER, 128));
    // The legitimate uses keep working.
    char cwd[128];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        syslog(LOG_ERR, "[t_userptr] getcwd into a real buffer: %s", strerror(errno));
        return -1;
    }
    if (stat("/home/user", &st) < 0) {
        syslog(LOG_ERR, "[t_userptr] stat into a real buffer: %s", strerror(errno));
        return -1;
    }
    utsname_t uts;
    if (uname(&uts) < 0) {
        syslog(LOG_ERR, "[t_userptr] uname into a real buffer: %s", strerror(errno));
        return -1;
    }
    old = signal(SIGUSR1, SIG_IGN);
    if (old == SIG_ERR) {
        syslog(LOG_ERR, "[t_userptr] signal control: %s", strerror(errno));
        return -1;
    }
    (void)sched_getparam(0, &sparam);
    return 0;
}

/// @brief The path-string gates: a path outside the caller's memory is
///        refused before anything walks it (#191 stage two).
/// @return 0 on success, -1 on failure.
static int check_path_strings(void)
{
    const char *paths[] = {"/home/user/t_userptr.d/str.txt"};
    EXPECT_EFAULT("open of a kernel-area path", open((const char *)KERNEL_TOP, O_RDONLY, 0));
    EXPECT_EFAULT("open of an unmapped path", open((const char *)UNMAPPED_USER, O_RDONLY, 0));
    EXPECT_EFAULT("open of NULL", open(NULL, O_RDONLY, 0));
    EXPECT_EFAULT("creat of a kernel-area path", creat((const char *)KERNEL_TOP, 0644));
    EXPECT_EFAULT("unlink of a kernel-area path", unlink((const char *)KERNEL_TOP));
    EXPECT_EFAULT("mkdir of a kernel-area path", mkdir((const char *)KERNEL_TOP, 0755));
    EXPECT_EFAULT("rmdir of a kernel-area path", rmdir((const char *)KERNEL_TOP));
    EXPECT_EFAULT("chdir of a kernel-area path", chdir((const char *)KERNEL_TOP));
    EXPECT_EFAULT("chmod of a kernel-area path", chmod((const char *)KERNEL_TOP, 0644));
    EXPECT_EFAULT("chown of a kernel-area path", chown((const char *)KERNEL_TOP, 0, 0));
    EXPECT_EFAULT("lchown of a kernel-area path", lchown((const char *)KERNEL_TOP, 0, 0));
    EXPECT_EFAULT("symlink of a kernel-area target", symlink((const char *)KERNEL_TOP, paths[0]));
    EXPECT_EFAULT("symlink into a kernel-area linkname", symlink("/home/user/welcome.md", (const char *)KERNEL_TOP));
    EXPECT_EFAULT("readlink into the kernel area", readlink("/home/user/tmp.md", (char *)KERNEL_TOP, 64));
    char link_target[64] = {0};
    EXPECT_EFAULT("readlink of a kernel-area path", readlink((const char *)KERNEL_TOP, link_target, sizeof(link_target)));
    // A control for the family: a real create-and-remove round trip.
    int fd = creat(paths[0], 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_userptr] creat control: %s", strerror(errno));
        return -1;
    }
    close(fd);
    if (unlink(paths[0]) < 0) {
        syslog(LOG_ERR, "[t_userptr] unlink control: %s", strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief The IPC family: fixed unions, computed array lengths that must
///        not wrap, and the address arguments of the shared-memory calls
///        (#191 stage two).
/// @return 0 on success, -1 on failure.
static int check_ipc_pointers(void)
{
    int semid = semget(IPC_PRIVATE, 1, 0666);
    if (semid < 0) {
        syslog(LOG_ERR, "[t_userptr] semget control: %s", strerror(errno));
        return -1;
    }
    int shmid = shmget(IPC_PRIVATE, 4096, 0666);
    if (shmid < 0) {
        syslog(LOG_ERR, "[t_userptr] shmget control: %s", strerror(errno));
        semctl(semid, 0, IPC_RMID, (union semun *)0);
        return -1;
    }
    int msqid = msgget(IPC_PRIVATE, 0666);
    if (msqid < 0) {
        syslog(LOG_ERR, "[t_userptr] msgget control: %s", strerror(errno));
        semctl(semid, 0, IPC_RMID, (union semun *)0);
        shmctl(shmid, IPC_RMID, NULL);
        return -1;
    }

    struct sembuf sop = {0, 0, 0};
    struct msgbuf msg;
    EXPECT_EFAULT("semop from the kernel area", raw_semop(semid, (struct sembuf *)KERNEL_TOP, 1));
    EXPECT_EFAULT("semop from unmapped memory", raw_semop(semid, (struct sembuf *)UNMAPPED_USER, 1));
    EXPECT_EFAULT("semop with a wrapping count", raw_semop(semid, &sop, 0x40000000U));
    EXPECT_EFAULT("semctl into the kernel area", semctl(semid, 0, IPC_STAT, (union semun *)KERNEL_TOP));
    EXPECT_EFAULT("msgctl into the kernel area", msgctl(msqid, IPC_STAT, (struct msqid_ds *)KERNEL_TOP));
    EXPECT_EFAULT("msgsnd from the kernel area", msgsnd(msqid, (const void *)KERNEL_TOP, 8, IPC_NOWAIT));
    EXPECT_EFAULT("msgrcv into the kernel area", msgrcv(msqid, (void *)KERNEL_TOP, 8, 0, IPC_NOWAIT));
    EXPECT_EFAULT_PTR("shmat of a kernel-area address", shmat(shmid, KERNEL_TOP, 0));
    EXPECT_EFAULT("shmdt of a kernel-area address", shmdt(KERNEL_TOP));
    // The legitimate uses keep working.
    int failed = 0;
    if (semop(semid, &sop, 1) < 0) {
        syslog(LOG_ERR, "[t_userptr] semop control: %s", strerror(errno));
        failed = 1;
    }
    // The whole buffer is cleared first, then the type is set: msgsnd reads
    // mtype out of it, so clearing after the assignment would send a zero.
    memset(&msg, 0, sizeof(msg));
    msg.mtype = 1;
    if (msgsnd(msqid, &msg, 1, IPC_NOWAIT) < 0) {
        syslog(LOG_ERR, "[t_userptr] msgsnd control: %s", strerror(errno));
        failed = 1;
    }
    // The three objects are removed whatever happened above, so a failure
    // here does not leave them behind for the rest of the run.
    semctl(semid, 0, IPC_RMID, (union semun *)0);
    shmctl(shmid, IPC_RMID, NULL);
    msgctl(msqid, IPC_RMID, NULL);
    return failed ? -1 : 0;
}

/// @brief The directory-reading buffer (#191 stage two).
/// @details The address arguments of the memory family are not here:
/// `mmap` and `munmap` have no compiled libc wrapper, and `brk` is only
/// ever reached through `malloc`. `shmat` and `shmdt` are covered with the
/// rest of the IPC family.
/// @return 0 on success, -1 on failure.
static int check_remaining_pointers(void)
{
    static dirent_t entries[2];
    int dfd = open("/home/user", O_RDONLY | O_DIRECTORY, 0);
    if (dfd < 0) {
        syslog(LOG_ERR, "[t_userptr] open dir control: %s", strerror(errno));
        return -1;
    }
    EXPECT_EFAULT("getdents into the kernel area", getdents(dfd, (dirent_t *)KERNEL_TOP, sizeof(entries)));
    EXPECT_EFAULT("getdents into unmapped memory", getdents(dfd, (dirent_t *)UNMAPPED_USER, sizeof(entries)));
    EXPECT_EFAULT("getdents across the kernel boundary", getdents(dfd, (dirent_t *)((char *)KERNEL_TOP - 2), sizeof(entries)));
    if (getdents(dfd, entries, sizeof(entries)) < 0) {
        syslog(LOG_ERR, "[t_userptr] getdents control: %s", strerror(errno));
        close(dfd);
        return -1;
    }
    close(dfd);

    return 0;
}

/// @brief The driver pointers that do not look like pointers at the syscall
///        boundary: ioctl passes them as an opaque unsigned long, so only
///        the driver can gate them (#394).
/// @return 0 on success, -1 on failure.
static int check_ioctl_pointers(void)
{
    termios_t saved;
    int cfd = open("/proc/video", O_WRONLY, 0);
    if (cfd < 0) {
        syslog(LOG_ERR, "[t_userptr] open(/proc/video): %s", strerror(errno));
        return -1;
    }
    // TCGETS writes the structure through the pointer, TCSETS reads it.
    EXPECT_EFAULT("tcgetattr into the kernel area", tcgetattr(cfd, (termios_t *)KERNEL_TOP));
    EXPECT_EFAULT("tcgetattr across the kernel boundary", tcgetattr(cfd, (termios_t *)((char *)KERNEL_TOP - 2)));
    EXPECT_EFAULT("tcgetattr into unmapped memory", tcgetattr(cfd, (termios_t *)UNMAPPED_USER));
    EXPECT_EFAULT("tcgetattr into NULL", tcgetattr(cfd, NULL));
    EXPECT_EFAULT("tcsetattr from the kernel area", tcsetattr(cfd, 0, (const termios_t *)KERNEL_TOP));
    EXPECT_EFAULT("tcsetattr from unmapped memory", tcsetattr(cfd, 0, (const termios_t *)UNMAPPED_USER));
    // The legitimate round trip must keep working, and must leave the
    // terminal exactly as it was found.
    if (tcgetattr(cfd, &saved) < 0) {
        syslog(LOG_ERR, "[t_userptr] tcgetattr into a real buffer: %s", strerror(errno));
        close(cfd);
        return -1;
    }
    if (tcsetattr(cfd, 0, &saved) < 0) {
        syslog(LOG_ERR, "[t_userptr] tcsetattr from a real buffer: %s", strerror(errno));
        close(cfd);
        return -1;
    }
    close(cfd);
    return 0;
}

int main(void)
{
    int failures = 0;

    // The file every test can read.
    int rfd = open("/home/user/welcome.md", O_RDONLY, 0);
    if (rfd < 0) {
        syslog(LOG_ERR, "[t_userptr] open(welcome.md): %s", strerror(errno));
        return EXIT_FAILURE;
    }
    // The scratch file nobody minds rewriting.
    int wfd = creat("/home/user/t_userptr.d/scratch.txt", 0644);
    if (wfd < 0) {
        if ((mkdir("/home/user/t_userptr.d", 0755) < 0) && (errno != EEXIST)) {
            syslog(LOG_ERR, "[t_userptr] mkdir: %s", strerror(errno));
            close(rfd);
            return EXIT_FAILURE;
        }
        wfd = creat("/home/user/t_userptr.d/scratch.txt", 0644);
        if (wfd < 0) {
            syslog(LOG_ERR, "[t_userptr] creat: %s", strerror(errno));
            close(rfd);
            return EXIT_FAILURE;
        }
    }

    if (check_read_pointers(rfd) < 0) {
        ++failures;
    }
    if (check_write_pointers(wfd) < 0) {
        ++failures;
    }
    if (check_scalar_outputs() < 0) {
        ++failures;
    }
    if (check_scalar_pointers() < 0) {
        ++failures;
    }
    if (check_path_strings() < 0) {
        ++failures;
    }
    if (check_ipc_pointers() < 0) {
        ++failures;
    }
    if (check_remaining_pointers() < 0) {
        ++failures;
    }
    if (check_ioctl_pointers() < 0) {
        ++failures;
    }
    close(rfd);
    close(wfd);

    // Best-effort cleanup, so a second run on the same image starts clean.
    unlink("/home/user/t_userptr.d/scratch.txt");
    rmdir("/home/user/t_userptr.d");

    if (failures == 0) {
        syslog(LOG_INFO, "[t_userptr] all user-pointer checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_userptr] %d FAILURES", failures);
    return EXIT_FAILURE;
}
