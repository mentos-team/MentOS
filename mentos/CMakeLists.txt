# ------------------------------------------------------------------------------
# Initialize the project.
project(MentOs C ASM)

# ------------------------------------------------------------------------------
# Set the project name.
set(PROJECT_NAME MentOs)

# ------------------------------------------------------------------------------
# Find doxygen
find_package(Doxygen)

# Add option to generate documentation.
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if (BUILD_DOCUMENTATION)
    if (NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif (NOT DOXYGEN_FOUND)

    set(doxyfile_in ${CMAKE_SOURCE_DIR}/doc/dreamos.doxyfile)
    set(doxyfile ${CMAKE_BINARY_DIR}/doxyfile)

    configure_file(${doxyfile_in} ${doxyfile} @ONLY)

    add_custom_target(
        doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/html DESTINATION share/doc)
endif (BUILD_DOCUMENTATION)

# ------------------------------------------------------------------------------
# Add the scheduling option.
set(SCHEDULER_TYPE
    "SCHEDULER_RR"
    CACHE STRING
    "Chose the type of scheduler: SCHEDULER_RR SCHEDULER_PRIORITY SCHEDULER_CFS")
set_property(
    CACHE SCHEDULER_TYPE PROPERTY STRINGS
    SCHEDULER_RR
    SCHEDULER_PRIORITY
    SCHEDULER_CFS)
if ("${SCHEDULER_TYPE}" STREQUAL "SCHEDULER_RR" OR
    "${SCHEDULER_TYPE}" STREQUAL "SCHEDULER_PRIORITY" OR
    "${SCHEDULER_TYPE}" STREQUAL "SCHEDULER_CFS")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D${SCHEDULER_TYPE}")
    message(STATUS "Setting scheduler type to ${SCHEDULER_TYPE}.")
else ()
    message(FATAL_ERROR "Scheduler type ${SCHEDULER_TYPE} is not valid.")
endif ()

# ------------------------------------------------------------------------------
# Add the option to enable buddy system.
option(ENABLE_BUDDY_SYSTEM "Enables the buddy system" OFF)
if (ENABLE_BUDDY_SYSTEM)
    message(STATUS "Enabling buddy system.")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_BUDDYSYSTEM")
else (ENABLE_BUDDY_SYSTEM)
    message(STATUS "Buddy system disabled.")
endif (ENABLE_BUDDY_SYSTEM)

# ------------------------------------------------------------------------------
# Set the compiler options (original).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-stack-protector")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=i686")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# ------------------------------------------------------------------------------
# Set the compiler options (extra).
set(CMAKE_C_FLAGS"${CMAKE_C_FLAGS} -Wall")
#set(CMAKE_C_FLAGS"${CMAKE_C_FLAGS} -Wpedantic")
#set(CMAKE_C_FLAGS"${CMAKE_C_FLAGS} -pedantic-errors")
#set(CMAKE_C_FLAGS"${CMAKE_C_FLAGS} -Wextra")
#set(CMAKE_C_FLAGS"${CMAKE_C_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-align")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-qual")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdisabled-optimization")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfloat-equal")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfloat-conversion")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat=2")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-nonliteral")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-security")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat-y2k")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimport")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Winit-self")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Winline")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Winvalid-pch")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-long-long")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-field-initializers")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-format-attribute")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-include-dirs")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpacked")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wredundant-decls")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstack-protector")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wstrict-aliasing=2")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wswitch")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wswitch-default")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wswitch-enum")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunreachable-code")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-function")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-label")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-parameter")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-value")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-variable")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvariadic-macros")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wwrite-strings")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-compare")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-conversion")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wuninitialized")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fmessage-length=0")

# ------------------------------------------------------------------------------

if (CMAKE_BUILD_TYPE MATCHES "Release")
elseif (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")
endif (CMAKE_BUILD_TYPE MATCHES "Release")

# ------------------------------------------------------------------------------
# Set the assembly flags.
enable_language(ASM)
set(CMAKE_ASM_COMPILER ${CMAKE_SOURCE_DIR}/third_party/nasm/bin/nasm)
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -g")
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -O0")
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -f elf")
SET(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -F dwarf")
set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> <FLAGS> -o <OBJECT> <SOURCE>")

# ------------------------------------------------------------------------------
# Add the includes.
include_directories(
    inc
    inc/descriptor_tables
    inc/devices
    inc/drivers
    inc/drivers/keyboard
    inc/fs
    inc/hardware
    inc/io
    inc/kernel
    inc/libc
    inc/lng
    inc/mem
    inc/misc
    inc/sys
    inc/system
    inc/process
    inc/elf
    inc/ui
    inc/ui/shell
    inc/ui/command
    inc/ui/init
)

# ------------------------------------------------------------------------------
# Add the source files.
set(SOURCE_FILES
    boot.asm

    src/kernel.c
    src/multiboot.c

    src/devices/pci.c
    src/devices/fpu.c

    src/drivers/ata.c
    src/drivers/fdc.c
    src/drivers/mouse.c
    src/drivers/keyboard/keyboard.c
    src/drivers/keyboard/keymap.c

    src/fs/fcntl.c
    src/fs/initrd.c
    src/fs/vfs.c
    src/fs/read_write.c

    src/sys/module.c
    src/sys/unistd.c
    src/hardware/timer.c
    src/hardware/cpuid.c
    src/hardware/pic8259.c
    src/io/port_io.c
    src/io/mm_io.c
    src/io/video.c

    src/kernel/sys.c

    src/libc/ctype.c
    src/libc/stdio.c
    src/libc/string.c
    src/libc/vsprintf.c
    src/libc/queue.c
    src/libc/stdlib.c
    src/libc/spinlock.c
    src/libc/list.c
    src/libc/ordered_array.c
    src/libc/mutex.c
    src/libc/libgen.c
    src/libc/strerror.c
    src/libc/tree.c
    src/libc/bitset.c
    src/libc/fcvt.c
    src/libc/rbtree.c
    src/libc/math.c
    src/libc/hashmap.c

    src/libc/unistd/getppid.c
    src/libc/unistd/getpid.c
    src/libc/unistd/exit.c
    src/libc/unistd/vfork.c
    src/libc/unistd/read.c
    src/libc/unistd/write.c
    src/libc/unistd/execve.c
    src/libc/unistd/nice.c
    src/libc/unistd/open.c
    src/libc/unistd/reboot.c
    src/libc/unistd/waitpid.c
    src/libc/unistd/chdir.c
    src/libc/unistd/getcwd.c

    src/mem/kheap.c
    src/mem/paging.c
    src/mem/zone_allocator.c

    src/misc/bitops.c
    src/misc/clock.c
    src/misc/debug.c
    src/sys/dirent.c
    src/sys/stat.c
    src/sys/utsname.c
    #src/sys/shm.c
    src/elf/elf.c

    src/descriptor_tables/gdt.c
    src/descriptor_tables/gdt.asm
    src/descriptor_tables/interrupt.c
    src/descriptor_tables/exception.c
    src/descriptor_tables/interrupt.asm
    src/descriptor_tables/exception.asm
    src/descriptor_tables/idt.c
    src/descriptor_tables/idt.asm
    src/descriptor_tables/tss.c
    src/descriptor_tables/tss.asm

    src/process/scheduler.c
    src/process/scheduler_algorithm.c
    src/process/process.c
    src/process/user.asm

    src/system/errno.c
    src/system/panic.c
    src/system/syscall.c
    src/system/printk.c

    src/ui/command/cmd_cd.c
    src/ui/command/cmd_clear.c
    src/ui/command/cmd_cpuid.c
    src/ui/command/cmd_credits.c
    src/ui/command/cmd_date.c
    src/ui/command/cmd_drv_load.c
    src/ui/command/cmd_echo.c
    src/ui/command/cmd_logo.c
    src/ui/command/cmd_ls.c
    src/ui/command/cmd_mkdir.c
    src/ui/command/cmd_more.c
    src/ui/command/cmd_newfile.c
    src/ui/command/cmd_poweroff.c
    src/ui/command/cmd_ps.c
    src/ui/command/cmd_pwd.c
    src/ui/command/cmd_rm.c
    src/ui/command/cmd_rmdir.c
    src/ui/command/cmd_showpid.c
    src/ui/command/cmd_sleep.c
    #src/ui/command/cmd_tester.c
    src/ui/command/cmd_touch.c
    src/ui/command/cmd_uname.c
    src/ui/command/cmd_whoami.c
    src/ui/command/cmd_ipcs.c
    src/ui/command/cmd_ipcrm.c
    src/ui/command/cmd_nice.c

    src/ui/init/init.c

    src/ui/shell/shell.c
    src/ui/shell/shell_login.c
    )

if (ENABLE_BUDDY_SYSTEM MATCHES "ON")
    set(SOURCE_FILES
            ${SOURCE_FILES}
            src/mem/buddysystem.c)
endif (ENABLE_BUDDY_SYSTEM MATCHES "ON")

# ------------------------------------------------------------------------------
# Add the executable.
add_library(${PROJECT_NAME} ${SOURCE_FILES})

# ------------------------------------------------------------------------------
# Build the kernel.
add_custom_command(
    TARGET MentOs
    POST_BUILD
    COMMAND $(LD)
    -melf_i386
    -static
    --oformat elf32-i386
    -Ttext 0x100000
    --output=${CMAKE_CURRENT_BINARY_DIR}/kernel.bin
    --script=${CMAKE_CURRENT_SOURCE_DIR}/kernel.lds
    ${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}.a
    -Map ${CMAKE_CURRENT_BINARY_DIR}/kernel.map
)
