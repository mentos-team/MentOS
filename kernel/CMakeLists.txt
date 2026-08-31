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
option(ENABLE_KERNEL_TESTS "Enable kernel-side unit and integration tests" OFF)
option(ENABLE_PERF "Enables the perf counter facility (klib/perf.h)." OFF)
option(ENABLE_SCHEDULER_FEEDBACK "Enables scheduling feedback on terminal." OFF)

# =============================================================================
# Set the list of valid video backends.
#
# Exactly one backend is compiled into the kernel. Each entry maps to the
# backend source that defines the `video_backend` symbol and to the geometry
# header the generic console sizes its static state from. Resolved here,
# before the sources are collected, so the unselected backends can be filtered
# out below.
set(VIDEO_TYPES VGA_TEXT_MODE VGA_MODE_640_480_16 VBE_MODE_1024_768_8 VIRTIO_GPU)
# Add the video backend option.
set(VIDEO_TYPE "VGA_TEXT_MODE" CACHE STRING "Chose the type of video backend: ${VIDEO_TYPES}")
# List of video backends.
set_property(CACHE VIDEO_TYPE PROPERTY STRINGS ${VIDEO_TYPES})
# Check which video backend is currently active and select its sources.
list(FIND VIDEO_TYPES ${VIDEO_TYPE} VIDEO_TYPE_INDEX)
if(VIDEO_TYPE_INDEX EQUAL -1)
    message(FATAL_ERROR "Video type ${VIDEO_TYPE} is not valid. Choose one of: ${VIDEO_TYPES}")
elseif(VIDEO_TYPE STREQUAL "VGA_TEXT_MODE")
    set(VIDEO_BACKEND_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/io/video/vga_text.c")
    set(VIDEO_GEOMETRY_HEADER "io/video/vga_text_geometry.h")
elseif(VIDEO_TYPE STREQUAL "VGA_MODE_640_480_16")
    set(VIDEO_BACKEND_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/io/video/vga_graphics.c")
    set(VIDEO_GEOMETRY_HEADER "io/video/vga_graphics_geometry.h")
elseif(VIDEO_TYPE STREQUAL "VBE_MODE_1024_768_8")
    set(VIDEO_BACKEND_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/io/video/vbe_lfb.c")
    set(VIDEO_GEOMETRY_HEADER "io/video/vbe_lfb_geometry.h")
elseif(VIDEO_TYPE STREQUAL "VIRTIO_GPU")
    # Two backends: the machine boots on VBE, which needs nothing but port I/O
    # and a mapping, and is then handed over to virtio-gpu once PCI, the
    # allocator and paging are all up. The geometry is the same either way, so
    # the console does not notice the change.
    set(VIDEO_BACKEND_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/io/video/vbe_lfb.c")
    set(VIDEO_GEOMETRY_HEADER "io/video/vbe_lfb_geometry.h")
    set(VIDEO_EXTRA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/io/video/virtio_gpu.c")
endif()
# Every branch above must have named a backend source and a geometry header, and
# the source has to exist. Checking here rather than trusting the branches means
# a VIDEO_TYPE added to the list but not given a branch fails the configure step
# instead of producing a kernel with no `video_backend` symbol and a link error
# that says nothing about why.
if(NOT VIDEO_BACKEND_SOURCE OR NOT VIDEO_GEOMETRY_HEADER)
    message(FATAL_ERROR "Video type ${VIDEO_TYPE} has no backend source or geometry header mapping.")
endif()
if(NOT EXISTS "${VIDEO_BACKEND_SOURCE}")
    message(FATAL_ERROR "Video backend source ${VIDEO_BACKEND_SOURCE} does not exist.")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/inc/${VIDEO_GEOMETRY_HEADER}")
    message(FATAL_ERROR "Video geometry header ${VIDEO_GEOMETRY_HEADER} does not exist.")
endif()
# Notify the type of video backend.
message(STATUS "Setting video type to ${VIDEO_TYPE}.")

# =============================================================================
# Collect the kernel source files.
file(GLOB_RECURSE KERNEL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")
# Remove the tests from the kernel sources if the tests are disabled.
if(NOT ENABLE_KERNEL_TESTS)
    list(FILTER KERNEL_SOURCES EXCLUDE REGEX ".*/tests/.*")
endif()
# Remove boot.c and boot.S from kernel sources - they only belong in the bootloader
list(FILTER KERNEL_SOURCES EXCLUDE REGEX ".*boot\\.(c|S)$")
# Keep only the selected video backend: every backend defines the same
# `video_backend` symbol, so two reaching the link is a duplicate definition
# rather than a silent fallback.
#
# The exclusion covers the whole directory and the selected source is then added
# back explicitly, which is what makes "exactly one" a property of the build
# rather than of the file names. The previous pattern matched `vga_*.c` only, so
# a backend named anything else would have been linked in addition to the
# selected one.
list(FILTER KERNEL_SOURCES EXCLUDE REGEX "/io/video/[^/]*\\.c$")
list(APPEND KERNEL_SOURCES ${VIDEO_BACKEND_SOURCE} ${VIDEO_EXTRA_SOURCES})
# Prove it. Exactly one source may define `video_backend`, which is the boot
# backend; anything in VIDEO_EXTRA_SOURCES is a backend the kernel may promote to
# at runtime and defines its own differently-named symbol, so those are counted
# separately rather than being allowed to slip past the check.
list(LENGTH VIDEO_BACKEND_SOURCE VIDEO_BOOT_BACKEND_COUNT)
if(NOT VIDEO_BOOT_BACKEND_COUNT EQUAL 1)
    message(FATAL_ERROR "Expected exactly one boot video backend, got ${VIDEO_BOOT_BACKEND_COUNT}: ${VIDEO_BACKEND_SOURCE}")
endif()
list(LENGTH VIDEO_EXTRA_SOURCES VIDEO_EXTRA_COUNT)
math(EXPR VIDEO_EXPECTED_COUNT "1 + ${VIDEO_EXTRA_COUNT}")
set(VIDEO_BACKENDS_IN_BUILD ${KERNEL_SOURCES})
list(FILTER VIDEO_BACKENDS_IN_BUILD INCLUDE REGEX "/io/video/[^/]*\\.c$")
list(LENGTH VIDEO_BACKENDS_IN_BUILD VIDEO_BACKENDS_COUNT)
if(NOT VIDEO_BACKENDS_COUNT EQUAL VIDEO_EXPECTED_COUNT)
    message(FATAL_ERROR "Expected ${VIDEO_EXPECTED_COUNT} video source(s), got ${VIDEO_BACKENDS_COUNT}: ${VIDEO_BACKENDS_IN_BUILD}")
endif()
# Collect the kernel include directories.
set(KERNEL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/lib/inc)
# Create the kernel static library - explicitly include multiboot.c since it's needed by both kernel and bootloader
add_library(kernel ${KERNEL_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/src/multiboot.c")
# Add the includes.
target_include_directories(kernel PUBLIC ${KERNEL_INCLUDE_DIRS})
# Define that this code is kernel code.
target_compile_definitions(kernel PUBLIC __KERNEL__)
# Define the root of MentOS for the kernel code.
target_compile_definitions(kernel PUBLIC MENTOS_ROOT="${CMAKE_SOURCE_DIR}")
# Tell the C code which video backend is active, and where to find the geometry
# it was built against.
target_compile_definitions(kernel PUBLIC ${VIDEO_TYPE})
target_compile_definitions(kernel PUBLIC VIDEO_GEOMETRY_HEADER="${VIDEO_GEOMETRY_HEADER}")
# Tell kmain whether this build has a backend to promote to after boot.
if(VIDEO_EXTRA_SOURCES)
    target_compile_definitions(kernel PUBLIC VIDEO_PROMOTE_VIRTIO_GPU)
endif()
# If the emulator is set to output on a log file, tell that to the C code.
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    target_compile_definitions(kernel PUBLIC EMULATOR_OUTPUT_LOG)
endif()
# If we enabled the tests, tell that to the C code.
if(ENABLE_PERF)
    target_compile_definitions(kernel PUBLIC ENABLE_PERF)
endif()

if(ENABLE_KERNEL_TESTS)
    target_compile_definitions(kernel PUBLIC ENABLE_KERNEL_TESTS)
endif()

# =============================================================================
# Enables memory allocation tracing.
if(ENABLE_KMEM_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_KMEM_TRACE)
endif()

if(ENABLE_PAGE_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_PAGE_TRACE)
endif()

if(ENABLE_EXT2_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_EXT2_TRACE)
endif()

if(ENABLE_FILE_TRACE)
    target_compile_definitions(kernel PUBLIC ENABLE_FILE_TRACE)
endif()

# =============================================================================
# Enables scheduling feedback on terminal.
if(ENABLE_SCHEDULER_FEEDBACK)
    target_compile_definitions(kernel PUBLIC ENABLE_SCHEDULER_FEEDBACK)
endif()

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
