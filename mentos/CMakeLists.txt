# =============================================================================
# KERNEL COMPILATION SETUP
# =============================================================================

# =============================================================================
# OPTIONS
# =============================================================================

# Enables memory allocation tracing.
option(ENABLE_KMEM_TRACE "Enables kmalloc tracing." OFF)
option(ENABLE_PAGE_TRACE "Enables page allocation tracing." OFF)
option(ENABLE_EXT2_TRACE "Enables EXT2 allocation tracing." OFF)
option(ENABLE_FILE_TRACE "Enables vfs_file allocation tracing." OFF)
# Enables scheduling feedback on terminal.
option(ENABLE_SCHEDULER_FEEDBACK "Enables scheduling feedback on terminal." OFF)

# =============================================================================
# Add the kernel library with its sources.
add_library(
    kernel
    ${CMAKE_SOURCE_DIR}/mentos/src/kernel.c
    ${CMAKE_SOURCE_DIR}/mentos/src/resource_tracing.c
    ${CMAKE_SOURCE_DIR}/mentos/src/multiboot.c
    ${CMAKE_SOURCE_DIR}/mentos/src/devices/pci.c
    ${CMAKE_SOURCE_DIR}/mentos/src/devices/fpu.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/ata.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/mem.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/rtc.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/fdc.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/mouse.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/ps2.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/keyboard/keyboard.c
    ${CMAKE_SOURCE_DIR}/mentos/src/drivers/keyboard/keymap.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/attr.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/vfs.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/read_write.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/open.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/pipe.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/stat.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/readdir.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/procfs.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/ioctl.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/fcntl.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/namei.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/ext2.c
    ${CMAKE_SOURCE_DIR}/mentos/src/hardware/timer.c
    ${CMAKE_SOURCE_DIR}/mentos/src/hardware/cpuid.c
    ${CMAKE_SOURCE_DIR}/mentos/src/hardware/pic8259.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/debug.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/mm_io.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/video.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/stdio.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_video.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_running.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_feedback.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_system.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_ipc.c
    ${CMAKE_SOURCE_DIR}/mentos/src/io/vga/vga.c
    ${CMAKE_SOURCE_DIR}/mentos/src/ipc/ipc.c
    ${CMAKE_SOURCE_DIR}/mentos/src/ipc/msg.c
    ${CMAKE_SOURCE_DIR}/mentos/src/ipc/sem.c
    ${CMAKE_SOURCE_DIR}/mentos/src/ipc/shm.c
    ${CMAKE_SOURCE_DIR}/mentos/src/kernel/sys.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/assert.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/ctype.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/mutex.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/string.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/vsprintf.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/vscanf.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/time.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/libgen.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/strerror.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/math.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/fcvt.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/spinlock.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/stdlib.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/rbtree.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/ndtree.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/hashmap.c
    ${CMAKE_SOURCE_DIR}/mentos/src/klib/list.c
    ${CMAKE_SOURCE_DIR}/mentos/src/mem/kheap.c
    ${CMAKE_SOURCE_DIR}/mentos/src/mem/paging.c
    ${CMAKE_SOURCE_DIR}/mentos/src/mem/slab.c
    ${CMAKE_SOURCE_DIR}/mentos/src/mem/vmem_map.c
    ${CMAKE_SOURCE_DIR}/mentos/src/mem/zone_allocator.c
    ${CMAKE_SOURCE_DIR}/mentos/src/mem/buddy_system.c
    ${CMAKE_SOURCE_DIR}/mentos/src/elf/elf.c
    ${CMAKE_SOURCE_DIR}/mentos/src/crypt/sha256.c
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/gdt.c
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/gdt.S
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/interrupt.c
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/exception.c
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/interrupt.S
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/exception.S
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/idt.c
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/idt.S
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/tss.c
    ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/tss.S
    ${CMAKE_SOURCE_DIR}/mentos/src/process/scheduler_algorithm.c
    ${CMAKE_SOURCE_DIR}/mentos/src/process/scheduler_feedback.c
    ${CMAKE_SOURCE_DIR}/mentos/src/process/pid_manager.c
    ${CMAKE_SOURCE_DIR}/mentos/src/process/scheduler.c
    ${CMAKE_SOURCE_DIR}/mentos/src/process/process.c
    ${CMAKE_SOURCE_DIR}/mentos/src/process/wait.c
    ${CMAKE_SOURCE_DIR}/mentos/src/process/user.S
    ${CMAKE_SOURCE_DIR}/mentos/src/sys/utsname.c
    ${CMAKE_SOURCE_DIR}/mentos/src/sys/module.c
    ${CMAKE_SOURCE_DIR}/mentos/src/system/errno.c
    ${CMAKE_SOURCE_DIR}/mentos/src/system/panic.c
    ${CMAKE_SOURCE_DIR}/mentos/src/system/printk.c
    ${CMAKE_SOURCE_DIR}/mentos/src/system/signal.c
    ${CMAKE_SOURCE_DIR}/mentos/src/system/syscall.c
)

# Add the includes.
target_include_directories(
    kernel PUBLIC
    ${CMAKE_SOURCE_DIR}/mentos/inc
    ${CMAKE_SOURCE_DIR}/libc/inc
)

# Define that this code is kernel code.
target_compile_definitions(
    kernel PUBLIC
    __KERNEL__
)

target_compile_definitions(
    kernel PUBLIC
    MENTOS_ROOT="${CMAKE_SOURCE_DIR}"
)

# If the emulator is set to output on a log file, tell that to the C code.
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    target_compile_definitions(kernel PUBLIC EMULATOR_OUTPUT_LOG)
endif()

# =============================================================================
# Enables memory allocation tracing.
if(ENABLE_KMEM_TRACE)
target_compile_definitions(kernel PUBLIC ENABLE_KMEM_TRACE)
endif(ENABLE_KMEM_TRACE)
if(ENABLE_PAGE_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_PAGE_TRACE)
endif(ENABLE_PAGE_TRACE)
if(ENABLE_EXT2_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_EXT2_TRACE)
endif(ENABLE_EXT2_TRACE)
if(ENABLE_FILE_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_FILE_TRACE)
endif(ENABLE_FILE_TRACE)

# =============================================================================
# Enables scheduling feedback on terminal.
if(ENABLE_SCHEDULER_FEEDBACK)
    target_compile_definitions(kernel PUBLIC ENABLE_SCHEDULER_FEEDBACK)
endif(ENABLE_SCHEDULER_FEEDBACK)

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
    target_compile_definitions(kernel PUBLIC ${SCHEDULER_TYPE})
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
    target_compile_definitions(kernel PUBLIC ${VIDEO_TYPE})
    # Notify the type of video driver.
    message(STATUS "Setting vide type to ${VIDEO_TYPE}.")
endif()

# =============================================================================
# Set the list of valid keyboard mappings.
set(KEYMAP_TYPES KEYMAP_US KEYMAP_IT KEYMAP_DE)
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
    target_compile_definitions(kernel PUBLIC USE_${KEYMAP_TYPE})
    # Notify the type of keyboard mapping.
    message(STATUS "Setting keyboard mapping to ${KEYMAP_TYPE}.")
endif()

# =============================================================================
# Add bootloader library.
add_library(
    bootloader
    ${CMAKE_SOURCE_DIR}/mentos/src/boot.c
    ${CMAKE_SOURCE_DIR}/mentos/src/boot.S
)
# Add the includes.
target_include_directories(
    bootloader PUBLIC
    ${CMAKE_SOURCE_DIR}/mentos/inc
    ${CMAKE_SOURCE_DIR}/libc/inc
)
# Define that this code is kernel code.
target_compile_definitions(
    bootloader PUBLIC
    __KERNEL__
)
target_compile_definitions(
    bootloader PUBLIC
    MENTOS_ROOT="${CMAKE_SOURCE_DIR}"
)

# =============================================================================
# Build the kernel.
add_custom_target(
    bootloader.bin ALL
    COMMAND ${CMAKE_LINKER} --output kernel.bin     -melf_i386 -static --oformat elf32-i386 -z noexecstack --script ${CMAKE_CURRENT_SOURCE_DIR}/kernel.lds $<TARGET_FILE_NAME:kernel> -u kmain
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-i386 -B i386 kernel.bin kernel.bin.o
    COMMAND ${CMAKE_LINKER} --output bootloader.bin -melf_i386 -static --oformat elf32-i386 -z noexecstack --script ${CMAKE_CURRENT_SOURCE_DIR}/boot.lds $<TARGET_FILE_NAME:bootloader> kernel.bin.o
    DEPENDS kernel
    DEPENDS bootloader
)
