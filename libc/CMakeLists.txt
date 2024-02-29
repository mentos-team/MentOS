# =============================================================================
# LIBRARY
# =============================================================================

# Add the library.
add_library(
    libc
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/dup.c
    ${CMAKE_SOURCE_DIR}/libc/src/stdio.c
    ${CMAKE_SOURCE_DIR}/libc/src/ctype.c
    ${CMAKE_SOURCE_DIR}/libc/src/string.c
    ${CMAKE_SOURCE_DIR}/libc/src/stdlib.c
    ${CMAKE_SOURCE_DIR}/libc/src/math.c
    ${CMAKE_SOURCE_DIR}/libc/src/fcvt.c
    ${CMAKE_SOURCE_DIR}/libc/src/time.c
    ${CMAKE_SOURCE_DIR}/libc/src/strerror.c
    ${CMAKE_SOURCE_DIR}/libc/src/termios.c
    ${CMAKE_SOURCE_DIR}/libc/src/libgen.c
    ${CMAKE_SOURCE_DIR}/libc/src/vsprintf.c
    ${CMAKE_SOURCE_DIR}/libc/src/vscanf.c
    ${CMAKE_SOURCE_DIR}/libc/src/pwd.c
    ${CMAKE_SOURCE_DIR}/libc/src/grp.c
    ${CMAKE_SOURCE_DIR}/libc/src/sched.c
    ${CMAKE_SOURCE_DIR}/libc/src/readline.c
    ${CMAKE_SOURCE_DIR}/libc/src/setenv.c
    ${CMAKE_SOURCE_DIR}/libc/src/assert.c
    ${CMAKE_SOURCE_DIR}/libc/src/abort.c
    ${CMAKE_SOURCE_DIR}/libc/src/io/mm_io.c
    ${CMAKE_SOURCE_DIR}/libc/src/io/debug.c
    ${CMAKE_SOURCE_DIR}/libc/src/sys/ipc.c
    ${CMAKE_SOURCE_DIR}/libc/src/sys/unistd.c
    ${CMAKE_SOURCE_DIR}/libc/src/sys/errno.c
    ${CMAKE_SOURCE_DIR}/libc/src/sys/utsname.c
    ${CMAKE_SOURCE_DIR}/libc/src/sys/mman.c
    ${CMAKE_SOURCE_DIR}/libc/src/sys/ioctl.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/creat.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getppid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getpid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/exit.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/setsid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getsid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/setpgid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getpgid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/setgid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getgid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/setuid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getuid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/fork.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/read.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/write.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/exec.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/nice.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/open.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/reboot.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/waitpid.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/chdir.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getcwd.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/close.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/stat.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/rmdir.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/mkdir.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/unlink.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/getdents.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/lseek.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/kill.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/signal.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/interval.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/symlink.c
    ${CMAKE_SOURCE_DIR}/libc/src/unistd/readlink.c
    ${CMAKE_SOURCE_DIR}/libc/src/libc_start.c
    ${CMAKE_SOURCE_DIR}/libc/src/crt0.S
)

# Add the includes.
target_include_directories(libc PUBLIC inc)

# Remove the 'lib' prefix.
set_target_properties(libc PROPERTIES PREFIX "")
