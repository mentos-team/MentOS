# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)
# Initialize the project.
project(kernel C ASM)

# =============================================================================
# Set the main file names.
set(KERNEL_NAME       kernel)
set(BOOTLOADER_NAME   kernel_bootloader)
set(BUDDY_SYSTEM_NAME kernel_memory)
set(BUDDY_SYSTEM_FILE ${PROJECT_SOURCE_DIR}/src/mem/libbuddysystem.a)

# =============================================================================
# OPTIONS
# =============================================================================

# Add the memory option.
option(USE_BUDDY_SYSTEM "Build using the buddysystem written by the user." OFF)
# Enables cache tracing.
option(ENABLE_CACHE_TRACE "Enables cache tracing." OFF)
# Enables memory allocation tracing.
option(ENABLE_ALLOC_TRACE "Enables memory allocation tracing." OFF)


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
# SOURCES
# =============================================================================

# Define the list of files.
set(KERNEL_SOURCES
    src/kernel.c
    src/multiboot.c
    src/devices/pci.c
    src/devices/fpu.c
    src/drivers/ata.c
    src/drivers/rtc.c
    src/drivers/fdc.c
    src/drivers/mouse.c
    src/drivers/ps2.c
    src/drivers/keyboard/keyboard.c
    src/drivers/keyboard/keymap.c
    src/fs/vfs.c
    src/fs/read_write.c
    src/fs/open.c
    src/fs/stat.c
    src/fs/readdir.c
    src/fs/procfs.c
    src/fs/ioctl.c
    src/fs/namei.c
    src/fs/ext2.c
    src/hardware/timer.c
    src/hardware/cpuid.c
    src/hardware/pic8259.c
    src/io/port_io.c
    src/io/mm_io.c
    src/io/video.c
    src/io/stdio.c
    src/io/debug.c
    src/io/proc_video.c
    src/io/proc_running.c
    src/io/proc_system.c
    src/io/vga/vga.c
    src/ipc/msg.c
    src/ipc/sem.c
    src/ipc/shm.c
    src/kernel/sys.c
    src/klib/assert.c
    src/klib/ctype.c
    src/klib/mutex.c
    src/klib/string.c
    src/klib/vsprintf.c
    src/klib/vscanf.c
    src/klib/time.c
    src/klib/libgen.c
    src/klib/strerror.c
    src/klib/math.c
    src/klib/fcvt.c
    src/klib/spinlock.c
    src/klib/rbtree.c
    src/klib/ndtree.c
    src/klib/hashmap.c
    src/klib/list.c
    src/mem/kheap.c
    src/mem/paging.c
    src/mem/slab.c
    src/mem/vmem_map.c
    src/mem/zone_allocator.c
    src/elf/elf.c
    src/descriptor_tables/gdt.c
    src/descriptor_tables/gdt.S
    src/descriptor_tables/interrupt.c
    src/descriptor_tables/exception.c
    src/descriptor_tables/interrupt.S
    src/descriptor_tables/exception.S
    src/descriptor_tables/idt.c
    src/descriptor_tables/idt.S
    src/descriptor_tables/tss.c
    src/descriptor_tables/tss.S
    src/process/scheduler_algorithm.c
    src/process/scheduler.c
    src/process/process.c
    src/process/wait.c
    src/process/user.S
    src/sys/utsname.c
    src/sys/module.c
    src/system/errno.c
    src/system/panic.c
    src/system/printk.c
    src/system/signal.c
    src/system/syscall.c
)

# =============================================================================
# Add kernel library.
if(USE_BUDDY_SYSTEM)
    # Add the library with `buddysystem.c`.
    add_library(${KERNEL_NAME} ${KERNEL_SOURCES} src/mem/buddysystem.c)
else(USE_BUDDY_SYSTEM)
    # Add the library without `buddysystem.c`.
    add_library(${KERNEL_NAME} ${KERNEL_SOURCES})
    # Link the exercise libraries.
    target_link_libraries(${KERNEL_NAME} ${BUDDY_SYSTEM_FILE})
endif(USE_BUDDY_SYSTEM)
# Add the includes.
target_include_directories(
    ${KERNEL_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/mentos/inc
    ${CMAKE_SOURCE_DIR}/libc/inc
)
# Define that this code is kernel code.
target_compile_definitions(
    ${KERNEL_NAME} PUBLIC
    __KERNEL__
)

# =============================================================================
# Enables cache tracing.
if(ENABLE_CACHE_TRACE)
    target_compile_definitions(${KERNEL_NAME} PUBLIC ENABLE_CACHE_TRACE)
endif(ENABLE_CACHE_TRACE)

# =============================================================================
# Enables memory allocation tracing.
if(ENABLE_ALLOC_TRACE)
    target_compile_definitions(${KERNEL_NAME} PUBLIC ENABLE_ALLOC_TRACE)
endif(ENABLE_ALLOC_TRACE)

# =============================================================================
# Set the list of valid scheduling options.
set(SCHEDULER_TYPES SCHEDULER_RR SCHEDULER_PRIORITY SCHEDULER_CFS SCHEDULER_EDF SCHEDULER_RM SCHEDULER_AEDF)
# Add the scheduling option.
set(SCHEDULER_TYPE "SCHEDULER_RR" CACHE STRING "Chose the type of scheduler: ${SCHEDULER_TYPES}")
# List of schedulers.
set_property(CACHE SCHEDULER_TYPE PROPERTY STRINGS ${SCHEDULER_TYPES})
# Check which scheduler is currently active and export the related macro.
list(FIND SCHEDULER_TYPES ${SCHEDULER_TYPE} INDEX)
if(index EQUAL -1)
    message(FATAL_ERROR "Scheduler type ${SCHEDULER_TYPE} is not valid.")
else()
    # Add the define stating which scheduler is currently active.
    target_compile_definitions(${KERNEL_NAME} PUBLIC ${SCHEDULER_TYPE})
    # Notify the type of scheduler.
    message(STATUS "Setting scheduler type to ${SCHEDULER_TYPE}.")
endif()

# =============================================================================
# Set the list of valid video driver options.
set(VIDEO_TYPES VGA_TEXT_MODE VGA_MODE_320_200_256 VGA_MODE_640_480_16 VGA_MODE_720_480_16)
# Add the video driver option.
set(VIDEO_TYPE "VGA_TEXT_MODE" CACHE STRING "Chose the type of video driver: ${VIDEO_TYPES}")
# List of video tpes.
set_property(CACHE VIDEO_TYPE PROPERTY STRINGS ${VIDEO_TYPES})
# Check which video driver is currently active and export the related macro.
list(FIND VIDEO_TYPES ${VIDEO_TYPE} INDEX)
if(index EQUAL -1)
    message(FATAL_ERROR "Video type ${VIDEO_TYPE} is not valid.")
else()
    # Add the define stating which video driver is currently active.
    target_compile_definitions(${KERNEL_NAME} PUBLIC ${VIDEO_TYPE})
    # Notify the type of video driver.
    message(STATUS "Setting vide type to ${VIDEO_TYPE}.")
endif()

# =============================================================================
# Set the list of valid keyboard mappings.
set(KEYMAP_TYPES KEYMAP_US KEYMAP_IT)
# Add the keyboard mapping.
set(KEYMAP_TYPE "KEYMAP_US" CACHE STRING "Chose the type of keyboard mapping: ${KEYMAP_TYPES}")
# List of keyboard mappings.
set_property(CACHE KEYMAP_TYPE PROPERTY STRINGS ${KEYMAP_TYPES})
# Check which keyboard mapping is currently active and export the related macro.
list(FIND KEYMAP_TYPES ${KEYMAP_TYPE} INDEX)
if(index EQUAL -1)
    message(FATAL_ERROR "keyboard mapping ${KEYMAP_TYPE} is not valid.")
else()
    # Add the define stating which keyboard mapping is currently active.
    target_compile_definitions(${KERNEL_NAME} PUBLIC USE_${KEYMAP_TYPE})
    # Notify the type of keyboard mapping.
    message(STATUS "Setting keyboard mapping to ${KEYMAP_TYPE}.")
endif()

# =============================================================================
# Add bootloader library.
add_library(
    ${BOOTLOADER_NAME}
    src/boot.c
    src/boot.S
)
# Add the includes.
target_include_directories(
    ${BOOTLOADER_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/mentos/inc
    ${CMAKE_SOURCE_DIR}/libc/inc
)
# Define that this code is kernel code.
target_compile_definitions(
    ${BOOTLOADER_NAME} PUBLIC
    __KERNEL__
)

# =============================================================================
# Build the kernel.
add_custom_target(
    kernel-bootloader.bin ALL
    COMMAND ${CMAKE_LINKER} -melf_i386 -static --oformat elf32-i386 --output kernel.bin --script ${CMAKE_CURRENT_SOURCE_DIR}/kernel.lds -Map kernel.map -u kmain
            $<TARGET_FILE_NAME:${KERNEL_NAME}> ${BUDDY_SYSTEM_FILE}
    COMMAND objcopy -I binary -O elf32-i386 -B i386 kernel.bin kernel.bin.o
    COMMAND ${CMAKE_LINKER} -melf_i386 -static --oformat elf32-i386 --output kernel-bootloader.bin --script ${CMAKE_CURRENT_SOURCE_DIR}/boot.lds -Map kernel-bootloader.map
            kernel.bin.o $<TARGET_FILE_NAME:${BOOTLOADER_NAME}>
    DEPENDS ${KERNEL_NAME}
    DEPENDS ${BOOTLOADER_NAME}
)
