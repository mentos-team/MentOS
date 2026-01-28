# =============================================================================
# LIBRARY
# =============================================================================

# Add the library.
add_library(
    libc
    ${CMAKE_SOURCE_DIR}/lib/src/err.c
    ${CMAKE_SOURCE_DIR}/lib/src/shadow.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/dup.c
    ${CMAKE_SOURCE_DIR}/lib/src/stdio.c
    ${CMAKE_SOURCE_DIR}/lib/src/ctype.c
    ${CMAKE_SOURCE_DIR}/lib/src/string.c
    ${CMAKE_SOURCE_DIR}/lib/src/stdlib.c
    ${CMAKE_SOURCE_DIR}/lib/src/math.c
    ${CMAKE_SOURCE_DIR}/lib/src/time.c
    ${CMAKE_SOURCE_DIR}/lib/src/strerror.c
    ${CMAKE_SOURCE_DIR}/lib/src/termios.c
    ${CMAKE_SOURCE_DIR}/lib/src/libgen.c
    ${CMAKE_SOURCE_DIR}/lib/src/vsprintf.c
    ${CMAKE_SOURCE_DIR}/lib/src/vscanf.c
    ${CMAKE_SOURCE_DIR}/lib/src/pwd.c
    ${CMAKE_SOURCE_DIR}/lib/src/grp.c
    ${CMAKE_SOURCE_DIR}/lib/src/sched.c
    ${CMAKE_SOURCE_DIR}/lib/src/readline.c
    ${CMAKE_SOURCE_DIR}/lib/src/setenv.c
    ${CMAKE_SOURCE_DIR}/lib/src/assert.c
    ${CMAKE_SOURCE_DIR}/lib/src/abort.c
    ${CMAKE_SOURCE_DIR}/lib/src/syslog.c
    ${CMAKE_SOURCE_DIR}/lib/src/ndtree.c
    ${CMAKE_SOURCE_DIR}/lib/src/list.c
    ${CMAKE_SOURCE_DIR}/lib/src/hashmap.c
    ${CMAKE_SOURCE_DIR}/lib/src/crypt/sha256.c
    ${CMAKE_SOURCE_DIR}/lib/src/io/mm_io.c
    ${CMAKE_SOURCE_DIR}/lib/src/sys/ipc.c
    ${CMAKE_SOURCE_DIR}/lib/src/sys/unistd.c
    ${CMAKE_SOURCE_DIR}/lib/src/sys/errno.c
    ${CMAKE_SOURCE_DIR}/lib/src/sys/utsname.c
    ${CMAKE_SOURCE_DIR}/lib/src/sys/mman.c
    ${CMAKE_SOURCE_DIR}/lib/src/sys/ioctl.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/chmod.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/chown.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/creat.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getppid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getpid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/exit.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/fcntl.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/setsid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getsid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/setpgid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getpgid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/setgid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getgid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/setuid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getuid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/fork.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/read.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/write.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/exec.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/nice.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/open.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/pipe.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/reboot.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/waitpid.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/chdir.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getcwd.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/close.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/stat.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/rmdir.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/mkdir.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/unlink.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/getdents.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/lseek.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/kill.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/signal.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/interval.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/symlink.c
    ${CMAKE_SOURCE_DIR}/lib/src/unistd/readlink.c
    ${CMAKE_SOURCE_DIR}/lib/src/libc_start.c
    ${CMAKE_SOURCE_DIR}/lib/src/crt0.S
)

# Add the includes.
target_include_directories(libc PUBLIC inc)

# Remove the 'lib' prefix.
set_target_properties(libc PROPERTIES PREFIX "")

# If the emulator is set to output on a log file, tell that to the C code.
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    target_compile_definitions(libc PUBLIC EMULATOR_OUTPUT_LOG)
endif()

target_compile_definitions(
    libc PUBLIC
    MENTOS_ROOT="${CMAKE_SOURCE_DIR}"
)
