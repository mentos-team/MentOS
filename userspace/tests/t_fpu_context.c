/// @file t_fpu_context.c
/// @brief Verifies x87 register preservation across a blocked syscall.
/// @details Each child leaves a distinct value in the x87 register stack,
///          blocks in read(), and checks the value after the scheduler
///          resumes its kernel continuation.  The inline assembly is kept
///          deliberately small so the test observes the architectural FPU
///          state rather than a compiler-generated stack temporary.
/// @copyright (c) 2024-2026 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include <stdlib.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

static void fpu_push(double value)
{
    __asm__ __volatile__("fldl %0" : : "m"(value));
}

static double fpu_pop(void)
{
    double value;
    __asm__ __volatile__("fstpl %0" : "=m"(value));
    return value;
}

static int child_status(pid_t child)
{
    int status = 0;
    if (waitpid(child, &status, 0) != child) {
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static void child_main(int read_fd, double expected)
{
    char token = 0;
    fpu_push(expected);
    if (read(read_fd, &token, sizeof(token)) != 1 || token != 'F') {
        close(read_fd);
        exit(2);
    }

    double observed = fpu_pop();
    close(read_fd);
    if (observed != expected) {
        syslog(LOG_ERR, "[t_fpu_context] expected %.17g, got %.17g", expected, observed);
        exit(3);
    }
    exit(0);
}

int main(void)
{
    int first[2];
    int second[2];
    if (pipe(first) < 0 || pipe(second) < 0) {
        return 1;
    }

    pid_t child_one = fork();
    if (child_one == 0) {
        close(first[1]);
        close(second[0]);
        close(second[1]);
        child_main(first[0], 1.25);
    }
    if (child_one < 0) {
        return 1;
    }

    pid_t child_two = fork();
    if (child_two == 0) {
        close(second[1]);
        close(first[0]);
        close(first[1]);
        child_main(second[0], -9.5);
    }
    if (child_two < 0) {
        return 1;
    }

    close(first[0]);
    close(second[0]);
    char token = 'F';
    if (write(first[1], &token, sizeof(token)) != 1 || write(second[1], &token, sizeof(token)) != 1) {
        return 1;
    }
    close(first[1]);
    close(second[1]);

    int result = child_status(child_one) | child_status(child_two);
    if (result == 0) {
        syslog(LOG_INFO, "[t_fpu_context] x87 state survived blocked reads");
    }
    return result;
}
