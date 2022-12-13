# =============================================================================
# Author: Enrico Fraccaroli
# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)
# Initialize the project.
project(libc C ASM)

# =============================================================================
# ASSEMBLY
# =============================================================================

# Enable the assembly language.
enable_language(ASM)
# Find the NASM compiler.
find_program(ASM_COMPILER NAMES nasm HINTS /usr/bin/ /usr/local/bin/)
# Mark the variable ASM_COMPILER as advanced.
mark_as_advanced(ASM_COMPILER)
# Check that we have found the compiler.
if(NOT ASM_COMPILER)
    message(FATAL_ERROR "ASM compiler not found!")
endif(NOT ASM_COMPILER)
# Set the asm compiler.
set(CMAKE_ASM_COMPILER ${ASM_COMPILER})
# Set the assembly compiler flags.
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -f elf")
set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> <FLAGS> -o <OBJECT> <SOURCE>")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -O0")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -F dwarf")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -O3")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

# =============================================================================
# C WARNINGs
# =============================================================================

# Warning flags.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pedantic-errors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# Disable some specific warnings.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-variable")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces")

# Set the compiler options.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-pic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcommon")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=i686")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

# =============================================================================
# Add the library.
add_library(
    ${PROJECT_NAME}

    ${PROJECT_SOURCE_DIR}/src/stdio.c
    ${PROJECT_SOURCE_DIR}/src/ctype.c
    ${PROJECT_SOURCE_DIR}/src/string.c
    ${PROJECT_SOURCE_DIR}/src/stdlib.c
    ${PROJECT_SOURCE_DIR}/src/math.c
    ${PROJECT_SOURCE_DIR}/src/fcvt.c
    ${PROJECT_SOURCE_DIR}/src/time.c
    ${PROJECT_SOURCE_DIR}/src/strerror.c
    ${PROJECT_SOURCE_DIR}/src/termios.c
    ${PROJECT_SOURCE_DIR}/src/libgen.c
    ${PROJECT_SOURCE_DIR}/src/vsprintf.c
    ${PROJECT_SOURCE_DIR}/src/vscanf.c
    ${PROJECT_SOURCE_DIR}/src/pwd.c
    ${PROJECT_SOURCE_DIR}/src/grp.c
    ${PROJECT_SOURCE_DIR}/src/sched.c
    ${PROJECT_SOURCE_DIR}/src/setenv.c
    ${PROJECT_SOURCE_DIR}/src/assert.c
    ${PROJECT_SOURCE_DIR}/src/abort.c
    ${PROJECT_SOURCE_DIR}/src/io/mm_io.c
    ${PROJECT_SOURCE_DIR}/src/io/port_io.c
    ${PROJECT_SOURCE_DIR}/src/ipc/ipc.c
    ${PROJECT_SOURCE_DIR}/src/sys/unistd.c
    ${PROJECT_SOURCE_DIR}/src/sys/errno.c
    ${PROJECT_SOURCE_DIR}/src/sys/utsname.c
    ${PROJECT_SOURCE_DIR}/src/sys/ioctl.c
    ${PROJECT_SOURCE_DIR}/src/unistd/creat.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getppid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getpid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/exit.c
    ${PROJECT_SOURCE_DIR}/src/unistd/setsid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getsid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/setpgid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getpgid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/setgid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getgid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/setuid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getuid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/fork.c
    ${PROJECT_SOURCE_DIR}/src/unistd/read.c
    ${PROJECT_SOURCE_DIR}/src/unistd/write.c
    ${PROJECT_SOURCE_DIR}/src/unistd/exec.c
    ${PROJECT_SOURCE_DIR}/src/unistd/nice.c
    ${PROJECT_SOURCE_DIR}/src/unistd/open.c
    ${PROJECT_SOURCE_DIR}/src/unistd/reboot.c
    ${PROJECT_SOURCE_DIR}/src/unistd/waitpid.c
    ${PROJECT_SOURCE_DIR}/src/unistd/chdir.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getcwd.c
    ${PROJECT_SOURCE_DIR}/src/unistd/close.c
    ${PROJECT_SOURCE_DIR}/src/unistd/stat.c
    ${PROJECT_SOURCE_DIR}/src/unistd/rmdir.c
    ${PROJECT_SOURCE_DIR}/src/unistd/mkdir.c
    ${PROJECT_SOURCE_DIR}/src/unistd/unlink.c
    ${PROJECT_SOURCE_DIR}/src/unistd/getdents.c
    ${PROJECT_SOURCE_DIR}/src/unistd/lseek.c
    ${PROJECT_SOURCE_DIR}/src/unistd/kill.c
    ${PROJECT_SOURCE_DIR}/src/unistd/signal.c
    ${PROJECT_SOURCE_DIR}/src/unistd/interval.c
    ${PROJECT_SOURCE_DIR}/src/libc_start.c
    ${PROJECT_SOURCE_DIR}/src/debug.c
    ${PROJECT_SOURCE_DIR}/src/crt0.S
)

# Add the includes.
target_include_directories(${PROJECT_NAME} PUBLIC inc)

# Remove the 'lib' prefix.
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
