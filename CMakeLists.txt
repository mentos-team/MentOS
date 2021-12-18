# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 2.8)
# Initialize the project.
project(MentOs)

# =============================================================================
# Set the default build type to Debug.
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# =============================================================================
# Add operating system specific option.
message(STATUS "Crosscompiling : ${CMAKE_CROSSCOMPILING}")
message(STATUS "System name    : ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "Kernel version : ${CMAKE_SYSTEM_VERSION}")
if((${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin") OR APPLE)
    # Set the Apple MacOSx compilers.
    if(CMAKE_VERSION VERSION_LESS "3.6.0")
        include(CMakeForceCompiler)
        cmake_force_c_compiler(i386-elf-gcc Clang)
        cmake_force_cxx_compiler(i386-elf-g++ Clang)
    else()
        set(CMAKE_C_COMPILER i386-elf-gcc)
        set(CMAKE_CXX_COMPILER i386-elf-g++)
        set(CMAKE_AR i386-elf-ar)
    endif()
    # Speicfy the linker.
    set(CMAKE_LINKER i386-elf-ld)
    # Specify the linker flags.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
elseif((${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows") OR WIN32)
  # Windows set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -sdl)
else()
    # Generic Unix System
    find_program(LSB_RELEASE_EXEC lsb_release)
    execute_process(
    COMMAND "${LSB_RELEASE_EXEC}" --short --release
    OUTPUT_VARIABLE LSB_RELEASE_VERSION_SHORT
    OUTPUT_STRIP_TRAILING_WHITESPACE)
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
endif()

# =============================================================================
# Add the debugging option.
set(DEBUGGING_TYPE "DEBUG_STDIO" CACHE STRING "Chose the type of debugging: DEBUG_STDIO DEBUG_LOG")
set_property(CACHE DEBUGGING_TYPE PROPERTY STRINGS DEBUG_STDIO DEBUG_LOG)
if("${DEBUGGING_TYPE}" STREQUAL "DEBUG_STDIO" OR "${DEBUGGING_TYPE}" STREQUAL "DEBUG_LOG")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D${DEBUGGING_TYPE}")
    message(STATUS "Setting debugging type to ${DEBUGGING_TYPE}.")
else()
    message(FATAL_ERROR "Debugging type ${DEBUGGING_TYPE} is not valid.")
endif()

# =============================================================================
# Add the sub-directories.
add_subdirectory(programs)
add_subdirectory(programs/tests)
add_subdirectory(mentos)
add_subdirectory(libc)
add_subdirectory(doc/doxygen)

# =============================================================================
# Target to generate the EXT2 filesystem.
add_custom_target(filesystem
    BYPRODUCTS ${CMAKE_BINARY_DIR}/rootfs.img
    COMMAND echo "---------------------------------------------"
    COMMAND echo "Creating EXT2 filesystem..."
    COMMAND echo "---------------------------------------------"
    COMMAND mke2fs -L 'rootfs' -N 0 -d ${CMAKE_SOURCE_DIR}/files -m 5 -r 1 -t ext2 -v -F ${CMAKE_BINARY_DIR}/rootfs.img 32M
    COMMAND echo "---------------------------------------------"
    COMMAND echo "Done!"
    COMMAND echo "---------------------------------------------"
    DEPENDS all_programs all_tests
)
  
# =============================================================================
# Update GDB symbol file.
add_custom_target(
    gdb_file
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

# =============================================================================
# Builds the code and runs qemu with the built Os.
set(EMULATOR qemu-system-i386)
# Set the debug type.
if(${DEBUGGING_TYPE} STREQUAL DEBUG_LOG)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial file:serial.log)
elseif(${DEBUGGING_TYPE} STREQUAL DEBUG_STDIO)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial stdio)
endif(${DEBUGGING_TYPE} STREQUAL DEBUG_LOG)
# Set the type of video.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -vga std)
# Set the amount of memory.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -m 1096M)
# Set the EXT2 drive.
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -drive file=rootfs.img,format=raw)
# Set the kernel.
set(EMULATOR_KERNEL -kernel mentos/kernel-bootloader.bin)

add_custom_target(
    qemu
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} ${EMULATOR_KERNEL} ${EMULATOR_FS}
    DEPENDS kernel-bootloader.bin
    DEPENDS all_programs
    DEPENDS all_tests
    DEPENDS libc
)

# =============================================================================
# Builds the code and runs qemu with the built Os.
add_custom_target(
    qemu-gdb
    COMMAND echo "\n\n"
    COMMAND echo "Now, QEMU has loaded the kernel, and it is waiting that you\n"
    COMMAND echo "remotely connect to it. To start debugging, open a new shell\n"
    COMMAND echo "in THIS same folder, and just type :\n"
    COMMAND printf "    cgdb -q -iex %q" "add-auto-load-safe-path ."
    COMMAND echo "\n\n"
    COMMAND ${EMULATOR} ${EMULATOR_KERNEL} ${EMULATOR_FS} -s -S
    DEPENDS kernel-bootloader.bin
    DEPENDS all_programs
    DEPENDS all_tests
    DEPENDS libc
    DEPENDS gdb_file
)
