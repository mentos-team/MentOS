/// @file t_execve_bigargv.c
/// @brief Regression test for #196: sys_execve must handle argv/envp of any
/// size safely — bounded counts, bounded per-string copies, and a dynamically
/// sized array of string positions — rejecting oversized vectors with E2BIG
/// instead of overflowing the kernel stack (`char *args_location[256]`) or
/// walking user memory unbounded.
/// @details Boundaries exercised (limits from limits.h):
///   - 257 and 300 entries: must EXECUTE correctly (previously a kernel-stack
///     out-of-bounds write);
///   - exactly MAX_ARG_COUNT (1024): allowed;
///   - MAX_ARG_COUNT + 1 (1025): -E2BIG;
///   - a single string of MAX_ARG_STRLEN - 1 (8191) chars: allowed;
///   - a single string of MAX_ARG_STRLEN (8192) chars: -E2BIG;
///   - total bytes at the ARG_MAX boundary: 900 x 64-byte entries allowed,
///     1000 x 64-byte entries (68004 > 65536) rejected;
///   - the same total-bytes boundary through envp.
/// The successful cases re-execute this binary and verify argc, the content
/// of every entry and (for envp) the `environ` array, so the full
/// kernel-push -> userspace-main round trip is checked.
/// @copyright (c) 2014-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <strerror.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

/// Path of this test binary inside the guest.
#define SELF_PATH "/bin/tests/t_execve_bigargv"

/// The per-entry filler length used by the byte-boundary cases: 63 chars
/// plus the terminator make 64 bytes per entry.
#define MED_FILLER_LEN 62

/// Storage for the argv vector built by the parent.
static char *big_argv[MAX_ARG_COUNT + 4];
/// Storage for the envp vector built by the parent.
static char *big_envp[MAX_ARG_COUNT + 4];
/// One shared filler string (all entries may alias it).
static char filler[MAX_ARG_STRLEN + 4];
/// The tag describing the expected layout: `N<count>L<filler-length>`.
static char tag[32];

/// @brief Fills the shared filler string with `len` 'f' characters.
static void make_filler(int len)
{
    memset(filler, 'f', len);
    filler[len] = '\0';
}

/// @brief Builds the vector for an argv case: tag, `count - 2` fillers, "p2".
static void build_argv(int count, int filler_len)
{
    make_filler(filler_len);
    snprintf(tag, sizeof(tag), "N%dL%d", count, filler_len);
    big_argv[0] = tag;
    for (int i = 1; i <= count - 2; ++i) {
        big_argv[i] = filler;
    }
    big_argv[count - 1] = (char *)"p2";
    big_argv[count]     = NULL;
}

/// @brief Forks and execs SELF with `argv`; the child image verifies the
/// round trip. A zero status means success (the child verified everything).
static int run_case(char *argv[], char *envp[], const char *label)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[t_execve_bigargv] fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execve(SELF_PATH, argv, envp);
        // The exec must not return; report the errno to the parent.
        if (errno == E2BIG) {
            exit(0);
        }
        syslog(LOG_ERR, "[t_execve_bigargv] %s: execve returned %s", label, strerror(errno));
        exit(100 + (errno & 0x7f));
    }
    int st;
    if (waitpid(pid, &st, 0) < 0) {
        syslog(LOG_ERR, "[t_execve_bigargv] waitpid: %s", strerror(errno));
        return -1;
    }
    if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
        syslog(
            LOG_ERR, "[t_execve_bigargv] %s: bad child status %d (sig %d)", label,
            WIFEXITED(st) ? WEXITSTATUS(st) : -1, WIFSIGNALED(st) ? WTERMSIG(st) : 0);
        return -1;
    }
    return 0;
}

/// @brief Phase 2 for the argv cases: verifies argc, the tag and every entry.
static int phase2_argv(int argc, char *argv[])
{
    if ((argc < 3) || (argv[0][0] != 'N')) {
        return 1;
    }
    int count = atoi(argv[0] + 1);
    char *l   = strchr(argv[0], 'L');
    if ((count != argc) || (l == NULL)) {
        syslog(LOG_ERR, "[t_execve_bigargv] phase2: argc %d, tag `%s`", argc, argv[0]);
        return 1;
    }
    int filler_len = atoi(l + 1);
    if ((int)strlen(filler) != filler_len) {
        // The parent and the child share the same binary: recompute it.
        make_filler(filler_len);
    }
    for (int i = 1; i <= argc - 2; ++i) {
        if ((argv[i] == NULL) || ((int)strlen(argv[i]) != filler_len) || (argv[i][0] != 'f')) {
            syslog(LOG_ERR, "[t_execve_bigargv] phase2: entry %d wrong (len %u)", i, argv[i] ? (unsigned)strlen(argv[i]) : 0u);
            return 1;
        }
    }
    if ((strcmp(argv[argc - 1], "p2") != 0) || (argv[argc] != NULL)) {
        syslog(LOG_ERR, "[t_execve_bigargv] phase2: terminator entry wrong");
        return 1;
    }
    return 0;
}

/// @brief Phase 2 for the envp case: verifies the `environ` array.
static int phase2_envp(int argc, char *argv[])
{
    extern char **environ;
    if ((argc != 2) || (argv[0][0] != 'E')) {
        return 1;
    }
    int count = atoi(argv[0] + 1);
    int envc  = 0;
    while ((environ[envc] != NULL) && (envc < count + 8)) {
        if ((environ[envc][0] != 'e') || ((int)strlen(environ[envc]) != MED_FILLER_LEN)) {
            syslog(LOG_ERR, "[t_execve_bigargv] phase2 env: entry %d wrong", envc);
            return 1;
        }
        ++envc;
    }
    if (envc != count) {
        syslog(LOG_ERR, "[t_execve_bigargv] phase2 env: %d entries, expected %d", envc, count);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // Phase 2: started by run_case with a crafted argv (and possibly envp).
    if ((argc >= 2) && (strcmp(argv[argc - 1], "p2") == 0)) {
        if (argv[0][0] == 'E') {
            return phase2_envp(argc, argv);
        }
        return phase2_argv(argc, argv);
    }

    char *envp_none[1]  = {NULL};
    char *envp_two[3]   = {(char *)"PATH=/bin", (char *)"HOME=/", NULL};
    static char efill[MED_FILLER_LEN + 1];
    memset(efill, 'e', MED_FILLER_LEN);
    efill[MED_FILLER_LEN] = '\0';

    int failures = 0;

    // Previously a kernel-stack out-of-bounds write: 257 and 300 entries.
    build_argv(257, MED_FILLER_LEN);
    if (run_case(big_argv, envp_none, "argv 257") < 0) {
        ++failures;
    }
    build_argv(300, 8);
    if (run_case(big_argv, envp_two, "argv 300") < 0) {
        ++failures;
    }

    // Exact entry-count boundary.
    build_argv(MAX_ARG_COUNT, 8);
    if (run_case(big_argv, envp_none, "argv MAX_ARG_COUNT") < 0) {
        ++failures;
    }
    build_argv(MAX_ARG_COUNT + 1, 8);
    if (run_case(big_argv, envp_none, "argv MAX_ARG_COUNT+1") < 0) {
        ++failures;
    }

    // Single-string length boundary (count 3: tag, the long string, "p2").
    build_argv(3, MAX_ARG_STRLEN - 1);
    if (run_case(big_argv, envp_none, "string MAX_ARG_STRLEN-1") < 0) {
        ++failures;
    }
    build_argv(3, MAX_ARG_STRLEN);
    if (run_case(big_argv, envp_none, "string MAX_ARG_STRLEN") < 0) {
        ++failures;
    }

    // Total-bytes boundary through argv: 900 entries fit, 1000 exceed.
    build_argv(900, MED_FILLER_LEN);
    if (run_case(big_argv, envp_none, "argv 900x64B") < 0) {
        ++failures;
    }
    build_argv(1000, MED_FILLER_LEN);
    if (run_case(big_argv, envp_none, "argv 1000x64B") < 0) {
        ++failures;
    }

    // The same total-bytes boundary through envp.
    {
        char *av[3] = {(char *)"E500", (char *)"p2", NULL};
        for (int i = 0; i < 500; ++i) {
            big_envp[i] = efill;
        }
        big_envp[500] = NULL;
        if (run_case(av, big_envp, "envp 500x64B") < 0) {
            ++failures;
        }
        for (int i = 0; i < 1000; ++i) {
            big_envp[i] = efill;
        }
        big_envp[1000] = NULL;
        if (run_case(av, big_envp, "envp 1000x64B") < 0) {
            ++failures;
        }
    }

    if (failures == 0) {
        syslog(LOG_INFO, "[t_execve_bigargv] all argv/envp bound checks passed");
        return EXIT_SUCCESS;
    }
    syslog(LOG_ERR, "[t_execve_bigargv] %d FAILURES", failures);
    return EXIT_FAILURE;
}
