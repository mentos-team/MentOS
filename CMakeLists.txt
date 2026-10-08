# =============================================================================
# PROJECT SETUP
# =============================================================================

# Set the minimum required version of cmake.
# Target-scoped linker options require CMake 3.13 or newer.
cmake_minimum_required(VERSION 3.13...3.22)

# Initialize the project.
project(mentos C ASM)

# Add the CMAKE_BUILD_TYPE option with the full list of possible values.
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build: Debug, Release, RelWithDebInfo, MinSizeRel")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")

# -----------------------------------------------------------------------------
# ENABLE FETCH CONTENT
# -----------------------------------------------------------------------------

# We need this in order to import external projects.
include(FetchContent)
# Hide fetchcontent variables.
mark_as_advanced(FORCE
    FETCHCONTENT_QUIET
    FETCHCONTENT_BASE_DIR
    FETCHCONTENT_FULLY_DISCONNECTED
    FETCHCONTENT_UPDATES_DISCONNECTED
)

# -----------------------------------------------------------------------------
# DEPENDENCY (SYSTEM LIBRARIES)
# -----------------------------------------------------------------------------

find_package(Doxygen)

include(${CMAKE_SOURCE_DIR}/cmake/mentos-tools.cmake)

# =============================================================================
# OS-SPECIFIC COMPILERS SETUP
# =============================================================================

# Add operating system specific option.
message(STATUS "Crosscompiling : ${CMAKE_CROSSCOMPILING}")
message(STATUS "System name    : ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "Kernel version : ${CMAKE_SYSTEM_VERSION}")
if((${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin") OR APPLE)
    # Specify the linker flags.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
elseif((${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows") OR WIN32)
    # Windows set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -sdl)
else()
    # Generic Unix System.
    # Find the `lsb_release` program.
    find_program(LSB_RELEASE_EXEC lsb_release HINTS /usr/bin/ /usr/local/bin/)
    mark_as_advanced(LSB_RELEASE_EXEC)

    if(LSB_RELEASE_EXEC)
        execute_process(
            COMMAND "${LSB_RELEASE_EXEC}" --short --release
            OUTPUT_VARIABLE LSB_RELEASE_VERSION_SHORT
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "LSB version    : ${LSB_RELEASE_VERSION_SHORT}")

        # Use GTK display for Ubuntu 19+
        if(LSB_RELEASE_VERSION_SHORT MATCHES "^(19|2[0-9])")
            set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -display gtk)
        endif()
    endif()
endif()

# =============================================================================
# EMULATION SERIAL OUTPUT OPTION
# =============================================================================
# Set the list of valid emulator output options.
set(EMULATOR_OUTPUT_TYPES OUTPUT_STDIO OUTPUT_LOG)
# Add the emulator output option.
set(EMULATOR_OUTPUT_TYPE "OUTPUT_STDIO" CACHE STRING "Chose the type of emulator output: ${EMULATOR_OUTPUT_TYPES}")
# List of emulator output options.
set_property(CACHE EMULATOR_OUTPUT_TYPE PROPERTY STRINGS ${EMULATOR_OUTPUT_TYPES})
# Check which emulator output option is currently active.
list(FIND EMULATOR_OUTPUT_TYPES ${EMULATOR_OUTPUT_TYPE} INDEX)
if(INDEX EQUAL -1)
    message(FATAL_ERROR "Emulator output type ${EMULATOR_OUTPUT_TYPE} is not valid.")
else()
    message(STATUS "Setting emulator output type to ${EMULATOR_OUTPUT_TYPE}.")
endif()

# =============================================================================
# GLOBAL COMPILATION FLAGS
# =============================================================================

# `pr_debug`/`pr_info`/`pr_notice` compile to nothing below a file's
# `__DEBUG_LEVEL__`, which is `LOGLEVEL_NOTICE` almost everywhere: -Wformat
# only checks code the compiler sees, so most log calls are never
# format-checked in a normal build (#365). Forcing every level to
# `LOGLEVEL_DEBUG` here, behind an option that defaults off, lets a dedicated
# CI job (and anyone locally) catch a mismatch before it joins that blind
# spot, without changing what a normal build compiles.
option(FORCE_DEBUG_LOGLEVEL "Force every pr_* log call to compile in, to catch format mismatches a normal build never checks (#365)." OFF)

# Target-scoped compiler and linker options shared by freestanding components.
include(${CMAKE_SOURCE_DIR}/cmake/mentos-options.cmake)

# Set the assembly compiler flags.
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -m32")

# =============================================================================
# SUB-DIRECTORIES SETUP
# =============================================================================

# Add the sub-directories.
add_subdirectory(boot)
add_subdirectory(kernel)
add_subdirectory(lib)
add_subdirectory(userspace)
include(${CMAKE_SOURCE_DIR}/cmake/mentos-filesystem.cmake)

# =============================================================================
# EMULATOR CONFIGURATION
# =============================================================================

# Set the display device the selected video backend needs.
#
# The backend is picked at configure time and each one drives different
# hardware, so one fixed device cannot serve all of them: a VIRTIO_GPU kernel
# started with `-vga std` finds no virtio device at all, keeps the VBE backend
# it booted on and runs perfectly well -- on the fallback path, not the one that
# was built. The device therefore follows VIDEO_TYPE.
#
# VIDEO_TYPE lives in the kernel's cache entry, which only exists once
# add_subdirectory(kernel) has run; fail loudly rather than quietly defaulting
# if this section is ever moved above it.
if(NOT VIDEO_TYPE)
    message(FATAL_ERROR "VIDEO_TYPE is not set: the emulator configuration must come after add_subdirectory(kernel).")
endif()
if(VIDEO_TYPE STREQUAL "VIRTIO_GPU")
    # `virtio-vga`, not `virtio-gpu-pci`: it is a virtio-gpu that also exposes
    # the VGA and Bochs VBE compatibility registers. The kernel boots on the VBE
    # backend and only promotes to virtio once paging and PCI are up, so the
    # compatibility half is what puts a console on screen in between.
    set(EMULATOR_VGA_ARGS -vga virtio)
    # A passing boot is not proof that virtio was under test, because the
    # fallback is silent and complete. Name the backend the console has to end
    # up on and let the wrapper look for the hand-over. A bare name, not a
    # pattern: it has to survive being passed through a generated makefile and a
    # shell, so it must contain nothing either of them would touch.
    set(EMULATOR_REQUIRE_BACKEND "virtio-gpu-2d")
else()
    set(EMULATOR_VGA_ARGS -vga std)
    set(EMULATOR_REQUIRE_BACKEND "")
endif()
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} ${EMULATOR_VGA_ARGS})
# The test wrapper builds its own command line, so it needs the same flags as
# one shell word.
string(REPLACE ";" " " EMULATOR_VGA_STRING "${EMULATOR_VGA_ARGS}")
# Set the amount of memory.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -m 1096M)
# Set the RTC to use local time.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -rtc base=localtime)
# Disables all default devices (e.g., serial ports, network cards, VGA
# adapters). Only devices we explicitly specify will be added.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -nodefaults)
# Disable reboot and shutdown.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -no-reboot)
# Set the debug type.
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial file:${CMAKE_BINARY_DIR}/serial.log)
elseif(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_STDIO)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial stdio)
endif()
# Set the EXT2 drive.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -drive file=${CMAKE_BINARY_DIR}/rootfs.img,format=raw,if=ide,index=0,media=disk)

# =============================================================================
# Booting with QEMU for fun
# =============================================================================

# This target boots the same GRUB image used by the ISO and test targets.
add_custom_target(
    qemu
    COMMAND test -e ${CMAKE_BINARY_DIR}/rootfs.img || ${CMAKE_COMMAND} -E cmake_echo_color --red "No filesystem file detected, you need to run: make filesystem"
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -boot d -cdrom ${CMAKE_BINARY_DIR}/cdrom.iso
    DEPENDS cdrom.iso
)

# =============================================================================
# Booting with QEMU+GDB for debugging
# =============================================================================

# First, we need to generate a GDB file containing all the symbols of all our
# executables.
add_custom_target(
    gdbinit
    BYPRODUCTS ${CMAKE_BINARY_DIR}/gdb.run
    # Create the generic gdb configuration.
    COMMAND echo "add-symbol-file ${CMAKE_BINARY_DIR}/mentos/kernel.bin" > ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "add-symbol-file ${CMAKE_BINARY_DIR}/mentos/bootloader.bin" >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND find ${CMAKE_SOURCE_DIR}/filesystem/bin -type f | xargs realpath | sed 's/^/add-symbol-file /' >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "break boot.c: boot_main" >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "break kernel.c: kmain" >> ${CMAKE_BINARY_DIR}/gdb.run
    # Create the GDB connection file.
    COMMAND echo "target remote localhost:1234" >> ${CMAKE_BINARY_DIR}/gdb.run
    DEPENDS ${CMAKE_BINARY_DIR}/mentos/bootloader.bin
    DEPENDS ${CMAKE_BINARY_DIR}/mentos/kernel.bin
    DEPENDS programs
    DEPENDS tests
    DEPENDS libc
)

# This target runs the emulator with the GRUB image and also
# the `-s -S` options. This basically tells the emulator to run the kernel in
# debug mode, and pause it up until a debugger connects to it.
add_custom_target(
    qemu-gdb
    COMMAND test -e ${CMAKE_BINARY_DIR}/rootfs.img || ${CMAKE_COMMAND} -E cmake_echo_color --red "No filesystem file detected, you need to run: make filesystem"
    COMMAND echo ""
    COMMAND echo "Now, QEMU has loaded the kernel, and it is waiting that you"
    COMMAND echo "remotely connect to it. To start debugging, open a new shell"
    COMMAND echo "in THIS same folder, and just type:"
    COMMAND echo "    gdb --quiet --command=gdb.run"
    COMMAND echo "or if you want to use cgdb, type:"
    COMMAND echo "    cgdb --quiet --command=gdb.run"
    COMMAND echo ""
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -s -S -boot d -cdrom ${CMAKE_BINARY_DIR}/cdrom.iso
    DEPENDS cdrom.iso
    DEPENDS gdbinit
)

# =============================================================================
# Booting with QEMU+GRUB
# =============================================================================

# Build one GRUB ISO from a selected configuration. Keeping staging and
# packaging in one helper prevents the normal, userspace-test, and kernel-test
# images from drifting apart when the boot artifact layout changes.
function(mentos_add_grub_iso target_name output_name staging_name grub_config)
    set(staging_dir ${CMAKE_BINARY_DIR}/${staging_name})
    set(grub_config_source ${CMAKE_SOURCE_DIR}/iso/boot/grub/${grub_config})

    add_custom_target(
        ${target_name}
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${staging_dir}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/iso ${staging_dir}
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/mentos/bootloader.bin ${staging_dir}/boot/bootloader.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/mentos/kernel.bin ${staging_dir}/boot/kernel.bin
        COMMAND ${CMAKE_COMMAND} -E copy ${grub_config_source} ${staging_dir}/boot/grub/grub.cfg
        COMMAND "${GRUB_MKRESCUE_EXE}" -o ${CMAKE_BINARY_DIR}/${output_name} ${staging_dir}
        DEPENDS bootloader.bin kernel.bin ${ARGN}
    )
endfunction()

mentos_add_grub_iso(cdrom.iso cdrom.iso iso grub.cfg)

# This third target runs the emualtor, but this time, the kernel binary file is
# inside the cdrom, so we will not pass the `-kernel` option. We will pass the
# `-cdrom` option and the `-boot d` option, all the other options will remain
# the same.
add_custom_target(
    qemu-grub
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -boot d -cdrom ${CMAKE_BINARY_DIR}/cdrom.iso
    DEPENDS cdrom.iso
)

# =============================================================================
# Booting with QEMU+GRUB for testing
# =============================================================================

# The userspace test image uses a different GRUB command line.
mentos_add_grub_iso(cdrom_test.iso cdrom_test.iso iso_test grub.cfg.runtests filesystem)

# This target runs the emulator, and executes the runtests binary as init process.
# Uses isa-debug-exit for explicit completion/failure signaling.
# The wrapper recognizes QEMU exit 33 as normal suite completion, validates the
# TAP output, and maps only a fully passing run to conventional host success (0).
add_custom_target(
    qemu-test
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-test ${CMAKE_BINARY_DIR} 600 "${EMULATOR_VGA_STRING}" "${EMULATOR}" "${EMULATOR_REQUIRE_BACKEND}"
    DEPENDS cdrom_test.iso
)

# =============================================================================
# Booting with QEMU+GRUB for the kernel unit tests
# =============================================================================

# The kernel unit-test image uses a command line that runs suites and exits via
# isa-debug-exit without creating an init process.
mentos_add_grub_iso(cdrom_kerneltest.iso cdrom_kerneltest.iso iso_kerneltest grub.cfg.kerneltests filesystem)

# These take seconds, not the minutes the userspace suite takes, so the timeout
# is short on purpose: a run that is still going after it is hung, not slow.
add_custom_target(
    qemu-kernel-test
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-kernel-test ${CMAKE_BINARY_DIR} 120 "${EMULATOR_VGA_STRING}" "${EMULATOR}"
    DEPENDS cdrom_kerneltest.iso
)

# -----------------------------------------------------------------------------
# CODE ANALYSIS
# -----------------------------------------------------------------------------

if(CLANG_TIDY_EXE)
    file(GLOB_RECURSE ALL_PROJECT_FILES
        "${PROJECT_SOURCE_DIR}/lib/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/lib/inc/*.h"
        "${PROJECT_SOURCE_DIR}/lib/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/lib/src/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/inc/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/src/*.c"
        "${PROJECT_SOURCE_DIR}/userspace/**/*.h"
        "${PROJECT_SOURCE_DIR}/userspace/*.h"
        "${PROJECT_SOURCE_DIR}/userspace/**/*.c"
        "${PROJECT_SOURCE_DIR}/userspace/*.c"
    )
    add_custom_target(
        ${PROJECT_NAME}_clang_tidy
        COMMAND ${CLANG_TIDY_EXE}
        --system-headers=0
        --p=${CMAKE_BINARY_DIR}
        ${ALL_PROJECT_FILES}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
    add_custom_target(
        ${PROJECT_NAME}_clang_tidy_fix
        COMMAND ${CLANG_TIDY_EXE}
        --system-headers=0
        --fix --fix-errors
        --p=${CMAKE_BINARY_DIR}
        ${ALL_PROJECT_FILES}
        COMMENT "Running clang-tidy-fix"
        VERBATIM
    )
endif()

# -----------------------------------------------------------------------------
# DOCUMENTATION
# -----------------------------------------------------------------------------

if(DOXYGEN_FOUND)

    # FetchContent: Doxygen Awesome CSS
    FetchContent_Declare(doxygenawesome
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
        GIT_TAG v2.4.2
    )
    FetchContent_MakeAvailable(doxygenawesome)

    # Hide FetchContent variables to avoid clutter in ccmake.
    mark_as_advanced(FORCE
        FETCHCONTENT_UPDATES_DISCONNECTED_DOXYGENAWESOME
        FETCHCONTENT_SOURCE_DIR_DOXYGENAWESOME
    )

    # Read the file with the version.
    file(READ ${PROJECT_SOURCE_DIR}/kernel/inc/version.h version_file)
    # Extract the version.
    string(REGEX MATCH "OS_MAJOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MAJOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MINOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MINOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MICRO_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MICRO_VERSION ${CMAKE_MATCH_1})

    # Customization: Doxygen Configuration
    set(DOXYGEN_WARN_FORMAT "$file:$line:1: $text")
    set(DOXYGEN_PROJECT_NAME "MentOS")
    set(DOXYGEN_PROJECT_BRIEF "The Mentoring Operating System")
    set(DOXYGEN_PROJECT_NUMBER "${OS_MAJOR_VERSION}.${OS_MINOR_VERSION}.${OS_MICRO_VERSION}")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
    set(DOXYGEN_SHOW_INCLUDE_FILES NO)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_MAN NO)
    set(DOXYGEN_ENABLE_PREPROCESSING YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_MACRO_EXPANSION YES)
    set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
    set(DOXYGEN_PREDEFINED "__attribute__((x))= _syscall0= _syscall0(x)= _syscall1(x)= _syscall2(x)= _syscall3(x)=")

    # Styling and UX enhancements using Doxygen Awesome
    set(DOXYGEN_HTML_HEADER ${doxygenawesome_SOURCE_DIR}/doxygen-custom/header.html)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET ${doxygenawesome_SOURCE_DIR}/doxygen-awesome.css)
    set(DOXYGEN_HTML_EXTRA_FILES
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-fragment-copy-button.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-paragraph-link.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-darkmode-toggle.js
    )

    # Set stricter warnings for better documentation quality
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)
    set(DOXYGEN_WARN_AS_ERROR NO CACHE STRING "Treat Doxygen warnings as errors")
    set_property(CACHE DOXYGEN_WARN_AS_ERROR PROPERTY STRINGS YES NO)

    # Add Doxygen documentation target.
    file(GLOB_RECURSE ALL_PROJECT_FILES
        "${PROJECT_SOURCE_DIR}/lib/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/lib/inc/*.h"
        "${PROJECT_SOURCE_DIR}/lib/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/lib/src/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/inc/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/src/*.c"
    )

    doxygen_add_docs(
        ${PROJECT_NAME}_documentation
        ${PROJECT_SOURCE_DIR}/README.md
        ${PROJECT_SOURCE_DIR}/LICENSE.md
        ${ALL_PROJECT_FILES}
        COMMENT "Generating Doxygen documentation"
    )
endif(DOXYGEN_FOUND)
