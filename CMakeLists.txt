# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)
# 
set(HAVE_FLAG_SEARCH_PATHS_FIRST 0)
# Initialize the project.
project(mentos C ASM)

# =============================================================================
# BUILD TYPE (Debug/Release)
# =============================================================================

# Set the default build type to Debug.
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# =============================================================================
# DOCUMENTATION
# =============================================================================
add_subdirectory(doc)

# =============================================================================
# OS-SPECIFIC COMPILERS
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
    # Mark the program path as advanced, we do not want it to appear as an option.
    mark_as_advanced(LSB_RELEASE_EXEC)
    # Get the version.
    execute_process(
        COMMAND "${LSB_RELEASE_EXEC}" --short --release
        OUTPUT_VARIABLE LSB_RELEASE_VERSION_SHORT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "LSB version    : ${LSB_RELEASE_VERSION_SHORT}")
    if(${LSB_RELEASE_VERSION_SHORT} MATCHES "^18")
        # Ubuntu 18 set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -sdl)
    elseif(${LSB_RELEASE_VERSION_SHORT} MATCHES "^19")
        # Ubuntu 19
        set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -display gtk)
    elseif(${LSB_RELEASE_VERSION_SHORT} MATCHES "^20")
        # Ubuntu 20
        set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -display gtk)
    else()
        # set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -sdl)
    endif()
    # Speicfy the linker.
    set(CMAKE_LINKER ld)
endif()

# =============================================================================
# COMPILATION
# =============================================================================

# Add the sub-directories.
add_subdirectory(programs)
add_subdirectory(programs/tests)
add_subdirectory(mentos)
add_subdirectory(libc)

# =============================================================================
# FILESYSTEM
# =============================================================================

# MentOS is compatible with EXT2 fileystems. This target generates an EXT2
# fileystem using the content of the `files` folder.
add_custom_target(filesystem
    BYPRODUCTS ${CMAKE_BINARY_DIR}/rootfs.img
    COMMAND echo '============================================================================='
    COMMAND echo 'Creating EXT2 filesystem...'
    COMMAND echo '============================================================================='
    COMMAND mkdir -p ${CMAKE_SOURCE_DIR}/files/proc
    COMMAND mkdir -p ${CMAKE_SOURCE_DIR}/files/dev
    COMMAND mke2fs -L 'rootfs' -N 0 -d ${CMAKE_SOURCE_DIR}/files -b 4096 -m 5 -r 1 -t ext2 -v -F ${CMAKE_BINARY_DIR}/rootfs.img 32M
    COMMAND echo '============================================================================='
    COMMAND echo 'Done!'
    COMMAND echo '============================================================================='
    DEPENDS all_programs all_tests
)

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
if(index EQUAL -1)
    message(FATAL_ERROR "Emulator output type ${EMULATOR_OUTPUT_TYPE} is not valid.")
else()
    message(STATUS "Setting emulator output type to ${EMULATOR_OUTPUT_TYPE}.")
endif()

# =============================================================================
# EMULATOR CONFIGURATION
# =============================================================================

# Set the emulator.
set(EMULATOR qemu-system-i386)
# Set the debug type.
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial file:${CMAKE_BINARY_DIR}/serial.log)
elseif(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_STDIO)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial stdio)
endif(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
# Set the type of video.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -vga std)
# Set the amount of memory.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -m 1096M)
# Set the EXT2 drive.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -drive file=${CMAKE_BINARY_DIR}/rootfs.img,format=raw)

# =============================================================================
# Booting with QEMU for fun
# =============================================================================

# This first target runs the emualtor passing the kernel binary file.
add_custom_target(
    qemu
    COMMAND test -e ${CMAKE_BINARY_DIR}/rootfs.img || ${CMAKE_COMMAND} -E cmake_echo_color --red "No filesystem file detected, you need to run: make filesystem"
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -kernel ${CMAKE_BINARY_DIR}/mentos/kernel-bootloader.bin
    DEPENDS kernel-bootloader.bin
)

# =============================================================================
# Booting with QEMU+GDB for debugging
# =============================================================================

# First, we need to generate a GDB file containing all the symbols of all our
# executables.
add_custom_target(
    gdbinit
    BYPRODUCTS ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/get_text_address.sh ${CMAKE_BINARY_DIR}/mentos/kernel.bin > ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/get_text_address.sh ${CMAKE_BINARY_DIR}/mentos/kernel-bootloader.bin >> ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/get_text_address.sh ${CMAKE_BINARY_DIR}/programs/tests/test_* >> ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/get_text_address.sh ${CMAKE_BINARY_DIR}/programs/prog_* >> ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND echo "break boot.c: boot_main" >> ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND echo "break kernel.c: kmain" >> ${CMAKE_BINARY_DIR}/.gdbinit
    COMMAND echo "target remote localhost:1234" >> ${CMAKE_BINARY_DIR}/.gdbinit
    DEPENDS kernel-bootloader.bin
    DEPENDS all_programs
    DEPENDS all_tests
    DEPENDS libc
)

# This second target runs the emualtor passing the kernel binary file, and also
# the `-s -S` options. This basically tells the emulator to run the kernel in
# debug mode, and pause it up until a debugger connects to it.
add_custom_target(
    qemu-gdb
    COMMAND test -e ${CMAKE_BINARY_DIR}/rootfs.img || ${CMAKE_COMMAND} -E cmake_echo_color --red "No filesystem file detected, you need to run: make filesystem"
    COMMAND echo "\n\n"
    COMMAND echo "Now, QEMU has loaded the kernel, and it is waiting that you\n"
    COMMAND echo "remotely connect to it. To start debugging, open a new shell\n"
    COMMAND echo "in THIS same folder, and just type :\n"
    COMMAND printf "    cgdb -q -iex %q" "add-auto-load-safe-path ."
    COMMAND echo "\n\n"
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -s -S -kernel ${CMAKE_BINARY_DIR}/mentos/kernel-bootloader.bin
    DEPENDS kernel-bootloader.bin
    DEPENDS .gdbinit
)

# =============================================================================
# Booting with QEMU+GRUB
# =============================================================================

# First, we need to build the ISO for the cdrom.
add_custom_target(
  cdrom.iso
  COMMAND cp -rf ${CMAKE_SOURCE_DIR}/iso .
  COMMAND cp ${CMAKE_BINARY_DIR}/mentos/kernel-bootloader.bin ${CMAKE_BINARY_DIR}/iso/boot
  COMMAND grub-mkrescue -o ${CMAKE_BINARY_DIR}/cdrom.iso ${CMAKE_BINARY_DIR}/iso
  DEPENDS kernel-bootloader.bin
)

# This third target runs the emualtor, but this time, the kernel binary file is
# inside the cdrom, so we will not pass the `-kernel` option. We will pass the
# `-cdrom` option and the `-boot d` option, all the other options will remain
# the same.
add_custom_target(
    qemu-grub
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -boot d -cdrom ${CMAKE_BINARY_DIR}/cdrom.iso
    DEPENDS cdrom.iso
)