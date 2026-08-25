/// @file t_elf_short.c
/// @brief Regression tests for the ELF classification of short files (#224).
/// @details `elf_check_file_type()` used to accept any `vfs_read()` result
///          different from -1, so empty files, short reads, and errors like
///          -EISDIR were classified through an uninitialized stack header.
///          The tests below prove that files shorter than a full ELF header
///          are deterministically rejected with ENOEXEC, while a complete
///          valid header is still classified (and then executed: the control
///          file has no PT_LOAD segments, so the child jumps to the
///          unmapped entry point and dies with SIGSEGV — proving the file
///          went through the whole execve pipeline instead of being
///          rejected at the classification stage).
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// Directory where the test files are created.
#define FILE_DIR "/home/user/"

/// A complete, valid 52-byte ELF32/i386 ET_EXEC header with no program
/// headers (e_phnum = 0, e_phoff = 0). It passes every classification and
/// load check, and its entry point (0x08048000) is not backed by any
/// segment, so executing it ends at the unmapped entry with SIGSEGV.
static const unsigned char valid_header[52] = {
    // e_ident: ELF magic, ELFCLASS32, ELFDATA2LSB, EV_CURRENT.
    0x7f, 'E', 'L', 'F', 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // e_type = ET_EXEC (2).
    0x02, 0x00,
    // e_machine = EM_386 (3).
    0x03, 0x00,
    // e_version = 1.
    0x01, 0x00, 0x00, 0x00,
    // e_entry = 0x08048000 (unmapped: no PT_LOAD segments follow).
    0x00, 0x80, 0x04, 0x08,
    // e_phoff = 0.
    0x00, 0x00, 0x00, 0x00,
    // e_shoff = 0.
    0x00, 0x00, 0x00, 0x00,
    // e_flags = 0.
    0x00, 0x00, 0x00, 0x00,
    // e_ehsize = 52, e_phentsize = 32, e_phnum = 0.
    0x34, 0x00,
    0x20, 0x00,
    0x00, 0x00,
    // e_shentsize = 0, e_shnum = 0, e_shstrndx = 0.
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

/// Builds the full path of a test file inside FILE_DIR.
/// @param path The output buffer receiving the path.
/// @param size The size of the output buffer.
/// @param name The file name inside FILE_DIR.
static void build_path(char *path, size_t size, const char *name)
{
    snprintf(path, size, FILE_DIR "%s", name);
}

/// Creates an executable file with the given content.
/// @param name The file name inside FILE_DIR.
/// @param content The file content.
/// @param len The length of the content.
/// @return 0 on success, -1 on error.
static int create_file(const char *name, const char *content, size_t len)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), name);
    unlink(path);
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (fd < 0) {
        syslog(LOG_ERR, "[t_elf_short] open(%s): %s", path, strerror(errno));
        return -1;
    }
    if ((len > 0) && (write(fd, content, len) != (ssize_t)len)) {
        syslog(LOG_ERR, "[t_elf_short] write(%s): %s", path, strerror(errno));
        close(fd);
        unlink(path);
        return -1;
    }
    close(fd);
    return 0;
}

/// Forks, execs the given file and waits for the child.
/// @return 0 if execve succeeded and the child exited with status 0,
///         the errno if execve returned cleanly in the child,
///         -1 if the child was killed by a signal,
///         -2 on fork/waitpid failure.
static int run_file(const char *name)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), name);
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_elf_short] fork: %s", strerror(errno));
        return -2;
    }
    if (pid == 0) {
        char *argv[]  = { (char *)path, NULL };
        char *envp[]  = { NULL };
        execve(path, argv, envp);
        // execve returned, report the errno through the exit status.
        exit(errno);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0) {
        syslog(LOG_ERR, "[t_elf_short] waitpid: %s", strerror(errno));
        return -2;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

/// Checks that an executable shorter than a full ELF header is rejected
/// with ENOEXEC: the classification must not consult any byte of the
/// (partially) uninitialized header buffer (#224).
/// @param name The file name inside FILE_DIR.
/// @param content The file content.
/// @param len The length of the content.
/// @param label The label of the test case (for logging).
/// @return 1 if the test passed, 0 otherwise.
static int check_short_file(const char *name, const char *content, size_t len, const char *label)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), name);
    if (create_file(name, content, len) < 0) {
        return 0;
    }
    int res = run_file(name);
    unlink(path);
    if (res != ENOEXEC) {
        syslog(LOG_ERR, "[t_elf_short] %s: expected ENOEXEC, got %d", label, res);
        return 0;
    }
    return 1;
}

/// Checks that a complete valid ELF header is still classified and
/// executed: since the control file has no PT_LOAD segments, the child
/// must reach the unmapped entry point and die with SIGSEGV, not exit
/// with ENOEXEC (which would mean the classifier wrongly rejected a
/// full read).
/// @return 1 if the test passed, 0 otherwise.
static int check_complete_file(void)
{
    char path[PATH_MAX];
    build_path(path, sizeof(path), "t_elf_short_valid.elf");
    if (create_file("t_elf_short_valid.elf", (const char *)valid_header, sizeof(valid_header)) < 0) {
        return 0;
    }
    int res = run_file("t_elf_short_valid.elf");
    unlink(path);
    if (res != -1) {
        syslog(LOG_ERR, "[t_elf_short] valid header: expected SIGSEGV at the entry point, got %d", res);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int failures = 0;

    // ------------------------------------------------------------------
    // 0) Control: a complete 52-byte ELF header must be classified as an
    //    ELF and executed (the child dies with SIGSEGV at the unmapped
    //    entry point). This proves the fix does not over-reject.
    // ------------------------------------------------------------------
    if (!check_complete_file()) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 1) An empty file must be rejected with ENOEXEC, without the
    //    classification reading the uninitialized header.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_empty.bin", "", 0, "empty file")) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 2) A one-byte file holding the first magic byte.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_1.bin", "\x7f", 1, "one-byte file")) {
        ++failures;
    }
    // ------------------------------------------------------------------
    // 3) A three-byte file holding a partial ELF magic.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_3.bin", "\x7f"
                                                   "EL",
                          3, "partial magic")) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 4) A sixteen-byte file holding the whole e_ident: the magic, class,
    //    data and version all match, but e_type is missing. A short read
    //    must still be rejected.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_ident.bin", (const char *)valid_header, 16, "ident only")) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 5) A twenty-byte file: e_ident + e_type + e_machine are all valid,
    //    e_version is missing. All the fields consulted by the classifier
    //    checks that come before e_version are valid, so only the strict
    //    read-length check rejects this file deterministically.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_20.bin", (const char *)valid_header, 20, "twenty-byte file")) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 6) A fifty-one-byte file: the whole header except the last byte.
    //    Every field consulted by the classification is present and valid,
    //    so the pre-#224 code classified this file as a valid ELF (the
    //    short read passed the `!= -1` guard) and executed it.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_51.bin", (const char *)valid_header, 51, "fifty-one-byte file")) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 7) The empty file again, after the valid header was classified and
    //    executed: the kernel stack now holds the bytes of a valid ELF
    //    header, so an uninitialized read would have a real chance of
    //    classifying the empty file. It must still be rejected.
    // ------------------------------------------------------------------
    if (!check_short_file("t_elf_short_empty2.bin", "", 0, "empty file after valid exec")) {
        ++failures;
    }

    // ------------------------------------------------------------------
    // 8) The control once more: repeated classifications must behave
    //    exactly like the first one.
    // ------------------------------------------------------------------
    if (!check_complete_file()) {
        ++failures;
    }

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
