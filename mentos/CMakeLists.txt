# =============================================================================
# KERNEL COMPILATION SETUP
# =============================================================================

# Set the target naming.
set(KERNEL_NAME       kernel)
set(BOOTLOADER_NAME   bootloader)
set(BUDDY_SYSTEM_FILE ${CMAKE_SOURCE_DIR}/mentos/src/mem/libbuddysystem.a)

# =============================================================================
# OPTIONS
# =============================================================================

# Add the memory option.
option(USE_BUDDY_SYSTEM "Build using the buddysystem written by the user." OFF)
# Enables cache tracing.
option(ENABLE_CACHE_TRACE "Enables cache tracing." OFF)
# Enables memory allocation tracing.
option(ENABLE_ALLOC_TRACE "Enables memory allocation tracing." OFF)
# Enables scheduling feedback on terminal.
option(ENABLE_SCHEDULER_FEEDBACK "Enables scheduling feedback on terminal." OFF)

# =============================================================================
# SOURCES
# =============================================================================

# Define the list of files.
set(KERNEL_SOURCES
    ${CMAKE_SOURCE_DIR}/mentos/src/kernel.c
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
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/vfs.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/read_write.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/open.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/stat.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/readdir.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/procfs.c
    ${CMAKE_SOURCE_DIR}/mentos/src/fs/ioctl.c
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
    ${CMAKE_SOURCE_DIR}/mentos/src/elf/elf.c
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
# Enables scheduling feedback on terminal.
if(ENABLE_SCHEDULER_FEEDBACK)
    target_compile_definitions(${KERNEL_NAME} PUBLIC ENABLE_SCHEDULER_FEEDBACK)
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
    ${BOOTLOADER_NAME}.bin ALL
    COMMAND ${CMAKE_LINKER} -melf_i386 -static --oformat elf32-i386 --output kernel.bin --script ${CMAKE_CURRENT_SOURCE_DIR}/kernel.lds -Map kernel.map -u kmain $<TARGET_FILE_NAME:${KERNEL_NAME}> ${BUDDY_SYSTEM_FILE}
    COMMAND objcopy -I binary -O elf32-i386 -B i386 kernel.bin kernel.bin.o
    COMMAND ${CMAKE_LINKER} -melf_i386 -static --oformat elf32-i386 --output ${BOOTLOADER_NAME}.bin --script ${CMAKE_CURRENT_SOURCE_DIR}/boot.lds -Map bootloader.map kernel.bin.o $<TARGET_FILE_NAME:${BOOTLOADER_NAME}>
    DEPENDS ${KERNEL_NAME}
    DEPENDS ${BOOTLOADER_NAME}
)
