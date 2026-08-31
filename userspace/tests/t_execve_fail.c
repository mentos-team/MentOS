/// @file t_execve_fail.c
/// @brief Regression test for #208: a failed `execve` must leave the caller
/// intact and usable.
/// @details The test builds an ELF file that passes every `execve`
/// pre-check (valid ELF32/i386/ET_EXEC header, execute bit set) but whose
/// only `PT_LOAD` segment is not backed by the file (the segment starts
/// past EOF). The failure is therefore detected only while the new image is
/// being loaded, which is exactly the window after the pre-checks.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

/// Path of the crafted executable.
#define BAD_ELF_PATH "/home/user/t_execve_fail.elf"

/// Path of a plain non-executable file.
#define NOEXEC_PATH "/home/user/t_execve_fail.txt"

/// A truncated ET_EXEC file: valid 52-byte ELF header + 32-byte program
/// header (84 bytes total). The single PT_LOAD segment declares
/// p_offset = 0x1000 and p_filesz = 0x1000, so the file content ends long
/// before the segment begins. The entry point (0x08049000) falls inside the
/// zero-initialized part of the segment.
static const unsigned char bad_elf[] = {
    // e_ident: ELF magic, ELFCLASS32, ELFDATA2LSB, EV_CURRENT.
    0x7f, 'E', 'L', 'F', 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // e_type = ET_EXEC (2).
    0x02, 0x00,
    // e_machine = EM_386 (3).
    0x03, 0x00,
    // e_version = 1.
    0x01, 0x00, 0x00, 0x00,
    // e_entry = 0x08049000.
    0x00, 0x90, 0x04, 0x08,
    // e_phoff = 52 (right after the header).
    0x34, 0x00, 0x00, 0x00,
    // e_shoff = 0.
    0x00, 0x00, 0x00, 0x00,
    // e_flags = 0.
    0x00, 0x00, 0x00, 0x00,
    // e_ehsize = 52, e_phentsize = 32, e_phnum = 1.
    0x34, 0x00,
    0x20, 0x00,
    0x01, 0x00,
    // e_shentsize = 0, e_shnum = 0, e_shstrndx = 0.
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    // Program header: PT_LOAD.
    // p_type = PT_LOAD (1).
    0x01, 0x00, 0x00, 0x00,
    // p_offset = 0x1000 (past the 84-byte EOF).
    0x00, 0x10, 0x00, 0x00,
    // p_vaddr = 0x08048000.
    0x00, 0x80, 0x04, 0x08,
    // p_paddr = 0x08048000.
    0x00, 0x80, 0x04, 0x08,
    // p_filesz = 0x1000.
    0x00, 0x10, 0x00, 0x00,
    // p_memsz = 0x2000.
    0x00, 0x20, 0x00, 0x00,
    // p_flags = PF_R | PF_X (5).
    0x05, 0x00, 0x00, 0x00,
    // p_align = 0x1000.
    0x00, 0x10, 0x00, 0x00,
};

/// @brief Writes the crafted ELF file and marks it as executable, plus a
/// plain non-executable file.
/// @return 0 on success, -1 on failure.
static int create_bad_elf(void)
{
    int fd = creat(NOEXEC_PATH, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_execve_fail] creat: %s: %s", NOEXEC_PATH, strerror(errno));
        return -1;
    }
    if (write(fd, "not an elf\n", 11) != 11) {
        syslog(LOG_ERR, "[t_execve_fail] write: %s: %s", NOEXEC_PATH, strerror(errno));
        close(fd);
        return -1;
    }
    if (close(fd) < 0) {
        syslog(LOG_ERR, "[t_execve_fail] close: %s: %s", NOEXEC_PATH, strerror(errno));
        return -1;
    }

    fd = creat(BAD_ELF_PATH, 0755);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_execve_fail] creat: %s: %s", BAD_ELF_PATH, strerror(errno));
        return -1;
    }
    if (write(fd, bad_elf, sizeof(bad_elf)) != (ssize_t)sizeof(bad_elf)) {
        syslog(LOG_ERR, "[t_execve_fail] write: %s: %s", BAD_ELF_PATH, strerror(errno));
        close(fd);
        return -1;
    }
    if (close(fd) < 0) {
        syslog(LOG_ERR, "[t_execve_fail] close: %s: %s", BAD_ELF_PATH, strerror(errno));
        return -1;
    }
    return 0;
}

/// @brief Exercises the pre-checked failure paths: they must fail without
/// touching the caller either.
/// @return 0 on success, -1 on failure.
static int check_early_failures(pid_t expected_pid)
{
    char *argv_foo[2]  = {"t_execve_fail", NULL};
    char *envp_none[1] = {NULL};

    // A NULL filename must give EFAULT (#238: used to be EPERM).
    errno = 0;
    if (execve(NULL, argv_foo, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_fail] execve of NULL filename unexpectedly succeeded");
        return -1;
    }
    if (errno != EFAULT) {
        syslog(LOG_ERR, "[t_execve_fail] execve of NULL filename: expected EFAULT, got %s", strerror(errno));
        return -1;
    }

    // A NULL argv must give EFAULT (#238: used to be EPERM).
    errno = 0;
    if (execve("/bin/echo", NULL, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_fail] execve with NULL argv unexpectedly succeeded");
        return -1;
    }
    if (errno != EFAULT) {
        syslog(LOG_ERR, "[t_execve_fail] execve with NULL argv: expected EFAULT, got %s", strerror(errno));
        return -1;
    }

    // An argv with no entries at all (argv[0] == NULL) must give EINVAL
    // (#238: used to be EPERM): the address is valid and readable, but this
    // kernel requires at least the program name.
    char *argv_empty[1] = {NULL};
    errno               = 0;
    if (execve("/bin/echo", argv_empty, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_fail] execve with empty argv unexpectedly succeeded");
        return -1;
    }
    if (errno != EINVAL) {
        syslog(LOG_ERR, "[t_execve_fail] execve with empty argv: expected EINVAL, got %s", strerror(errno));
        return -1;
    }

    // A file that does not exist must give ENOENT.
    errno = 0;
    if (execve("/no/such/file", argv_foo, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_fail] execve of missing file unexpectedly succeeded");
        return -1;
    }
    if (errno != ENOENT) {
        syslog(LOG_ERR, "[t_execve_fail] execve of missing file: expected ENOENT, got %s", strerror(errno));
        return -1;
    }

    // A file without the execute bit must give EACCES.
    errno = 0;
    if (execve(NOEXEC_PATH, argv_foo, envp_none) != -1) {
        syslog(LOG_ERR, "[t_execve_fail] execve of non-executable unexpectedly succeeded");
        return -1;
    }
    if ((errno != EACCES) && (errno != EPERM)) {
        syslog(LOG_ERR, "[t_execve_fail] execve of non-executable: expected EACCES, got %s", strerror(errno));
        return -1;
    }

    // The caller must still be the very same process.
    if (getpid() != expected_pid) {
        syslog(LOG_ERR, "[t_execve_fail] pid changed after failed execve");
        return -1;
    }
    return 0;
}

/// @brief Child body: the failed execve must leave this process running.
/// @return the child exit code (0 on success).
static int child_main(void)
{
    // Data used to prove that the old image is still alive and consistent.
    char stack_canary[32];
    char *heap_canary;
    pid_t my_pid = getpid();

    memset(stack_canary, 0xAB, sizeof(stack_canary));
    heap_canary = malloc(64);
    if (heap_canary == NULL) {
        syslog(LOG_ERR, "[t_execve_fail] malloc: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    memset(heap_canary, 0xCD, 64);

    // The failure paths before the pre-checks must already work.
    if (check_early_failures(my_pid) < 0) {
        free(heap_canary);
        return EXIT_FAILURE;
    }

    // The main event: an executable that passes the pre-checks (valid ELF
    // header, ET_EXEC, execute bit) but fails while the image is loaded.
    char *argv_bad[2]  = {"t_execve_fail", NULL};
    char *envp_none[1] = {NULL};
    errno              = 0;
    if (execve(BAD_ELF_PATH, argv_bad, envp_none) != -1) {
        // If we reach this point either the execve failed to report the
        // error (old behavior: fake success before jumping into the
        // destroyed image), or something loaded the truncated file.
        syslog(LOG_ERR, "[t_execve_fail] execve of bad ELF unexpectedly succeeded");
        free(heap_canary);
        return EXIT_FAILURE;
    }
    if (errno != ENOEXEC) {
        syslog(LOG_ERR, "[t_execve_fail] execve of bad ELF: expected ENOEXEC, got %s", strerror(errno));
        free(heap_canary);
        return EXIT_FAILURE;
    }

    // The caller must still be executing its own code, with its own memory.
    if (getpid() != my_pid) {
        syslog(LOG_ERR, "[t_execve_fail] pid changed after failed execve");
        free(heap_canary);
        return EXIT_FAILURE;
    }
    for (size_t i = 0; i < sizeof(stack_canary); ++i) {
        if ((unsigned char)stack_canary[i] != 0xAB) {
            syslog(LOG_ERR, "[t_execve_fail] stack memory corrupted after failed execve");
            free(heap_canary);
            return EXIT_FAILURE;
        }
    }
    for (size_t i = 0; i < 64; ++i) {
        if ((unsigned char)heap_canary[i] != 0xCD) {
            syslog(LOG_ERR, "[t_execve_fail] heap memory corrupted after failed execve");
            free(heap_canary);
            return EXIT_FAILURE;
        }
    }
    free(heap_canary);

    // A few more syscalls must keep working.
    if (time(NULL) < 0) {
        syslog(LOG_ERR, "[t_execve_fail] time: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    if (unlink(BAD_ELF_PATH) < 0) {
        syslog(LOG_ERR, "[t_execve_fail] unlink: %s: %s", BAD_ELF_PATH, strerror(errno));
        return EXIT_FAILURE;
    }
    if (unlink(NOEXEC_PATH) < 0) {
        syslog(LOG_ERR, "[t_execve_fail] unlink: %s: %s", NOEXEC_PATH, strerror(errno));
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "[t_execve_fail] process survived the failed execve");

    // Finally, a working execve must still succeed after the rollback: we
    // replace ourselves with /bin/echo; its exit status is checked by the
    // parent.
    char *argv_echo[3] = {"echo", "t_execve_fail: post-failure exec", NULL};
    execve("/bin/echo", argv_echo, envp_none);

    // Getting here means the execve of a good executable failed.
    syslog(LOG_ERR, "[t_execve_fail] execve /bin/echo: %s", strerror(errno));
    return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
    // Build the crafted executable.
    if (create_bad_elf() < 0) {
        return EXIT_FAILURE;
    }

    // Run the actual test inside a child, so that even a catastrophic
    // regression (process killed, kernel panic) is observable as a test
    // failure instead of taking down the whole test runner.
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_execve_fail] fork: %s", strerror(errno));
        unlink(BAD_ELF_PATH);
        unlink(NOEXEC_PATH);
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        exit(child_main());
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) {
        syslog(LOG_ERR, "[t_execve_fail] waitpid: %s", strerror(errno));
        unlink(BAD_ELF_PATH);
        unlink(NOEXEC_PATH);
        return EXIT_FAILURE;
    }
    if (!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
        if (WIFSIGNALED(status)) {
            syslog(LOG_ERR, "[t_execve_fail] child killed by signal %d", WTERMSIG(status));
        } else {
            syslog(LOG_ERR, "[t_execve_fail] child exited with %d", WEXITSTATUS(status));
        }
        unlink(BAD_ELF_PATH);
        unlink(NOEXEC_PATH);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
