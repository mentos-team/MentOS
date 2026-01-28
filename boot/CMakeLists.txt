# =============================================================================
# BOOTLOADER COMPILATION SETUP
# =============================================================================

# Collect the bootloader source files.
file(GLOB_RECURSE BOOTLOADER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S")

# Collect the bootloader include directories.
set(BOOTLOADER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/inc ${CMAKE_SOURCE_DIR}/kernel/inc ${CMAKE_SOURCE_DIR}/lib/inc)

# Create the bootloader static library
add_library(bootloader ${BOOTLOADER_SOURCES})

# Add the includes.
target_include_directories(bootloader PUBLIC ${BOOTLOADER_INCLUDE_DIRS})

# Define that this code is kernel code.
target_compile_definitions(bootloader PUBLIC __KERNEL__)

# Define the root of MentOS for the bootloader code.
target_compile_definitions(bootloader PUBLIC MENTOS_ROOT="${CMAKE_SOURCE_DIR}")

# =============================================================================
# Build the bootloader binary
# =============================================================================

# Build the kernel binary first (from kernel library)
add_custom_target(
    kernel.bin
    COMMAND ${CMAKE_LINKER} --output kernel.bin -melf_i386 -static --oformat elf32-i386 -z noexecstack --script ${CMAKE_CURRENT_SOURCE_DIR}/linker/kernel.lds $<TARGET_FILE:kernel> -u kmain
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/mentos
    DEPENDS kernel
    BYPRODUCTS ${CMAKE_BINARY_DIR}/mentos/kernel.bin
)

# Convert kernel binary to object file
add_custom_target(
    kernel.bin.o
    COMMAND ${CMAKE_OBJCOPY} -I binary -O elf32-i386 -B i386 kernel.bin kernel.bin.o
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/mentos
    DEPENDS kernel.bin
    BYPRODUCTS ${CMAKE_BINARY_DIR}/mentos/kernel.bin.o
)

# Build the bootloader binary
add_custom_target(
    bootloader.bin ALL
    COMMAND ${CMAKE_LINKER} --output bootloader.bin -melf_i386 -static --oformat elf32-i386 -z noexecstack --script ${CMAKE_CURRENT_SOURCE_DIR}/linker/boot.lds $<TARGET_FILE:bootloader> kernel.bin.o
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/mentos
    DEPENDS bootloader kernel.bin.o
    BYPRODUCTS ${CMAKE_BINARY_DIR}/mentos/bootloader.bin
)
