/// @file kill.c
/// @brief
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <strerror.h>
#include <ctype.h>
#include <debug.h>

static inline int is_number(char *s)
{
    size_t len = strlen(s);
    for (size_t i = 0; i < len; ++i)
        if (!isdigit(s[i]))
            return 0;
    return 1;
}

static inline void print_signal_list()
{
    for (int it = 1; it < (NSIG - 1); ++it) {
        printf("%6s ", strsignal(it));
        if ((it % 8) == 0)
            putchar('\n');
    }
}

static inline int get_signal(char *s)
{
    int signr = 0;
    if (is_number(s))
        signr = atoi(s);
    else {
        ++s;
        for (int it = 1; it < (NSIG - 1); ++it) {
            if (strcmp(s, strsignal(it)) == 0) {
                signr = it;
                break;
            }
        }
    }
    if ((signr <= 0) || (signr >= NSIG))
        signr = 0;
    return signr;
}

static inline int get_pid(char *s)
{
    int pid = atoi(s);
    return ((pid > 0) && (pid < PID_MAX_LIMIT)) ? pid : 0;
}

static inline int send_signal(int pid, int signr)
{
    int ret = kill(pid, signr);
    if (ret == -1) {
        printf("[%d] %5d failed sending signal %d (%s) : %s\n",
               ret, pid, signr, strsignal(signr), strerror(errno));
        return 0;
    }
    printf("[%d] %5d sent signal %d (%s).\n",
           ret, pid, signr, strsignal(signr));
    return 1;
}

int main(int argc, char **argv)
{
    int pid, signr;

    if (argc == 1) {
        printf("%s: not enough arguments.\n", argv[0]);
        puts("Type kill -l for a list of signals\n");
        return 0;
    }
    if (argc == 2) {
        if (strcmp(argv[1], "-l") == 0) {
            print_signal_list();
        } else if (is_number(argv[1])) {
            if ((pid = get_pid(argv[1]))) {
                send_signal(pid, SIGTERM);
            } else {
                printf("%s: not a valid pid `%s`\n", argv[0], argv[1]);
                return 1;
            }
        } else {
            printf("%s: unrecognized option `%s`\n", argv[0], argv[1]);
            puts("Type kill -l for a list of signals\n");
            return 1;
        }
    } else {
        if (!(signr = get_signal(argv[1]))) {
            printf("%s: unrecognized signal `%s`.\n", argv[0], argv[1]);
            return 1;
        }

        for (int i = 2; i < argc; ++i)
            if ((pid = get_pid(argv[i])))
                send_signal(pid, signr);
    }
    putchar('\n');
    return 0;
}
