# =============================================================================
# EMULATOR CONFIGURATION
# =============================================================================

# Set the list of valid emulator output options.
set(EMULATOR_OUTPUT_TYPES OUTPUT_STDIO OUTPUT_LOG)
set(EMULATOR_OUTPUT_TYPE "OUTPUT_STDIO" CACHE STRING "Chose the type of emulator output: ${EMULATOR_OUTPUT_TYPES}")
set_property(CACHE EMULATOR_OUTPUT_TYPE PROPERTY STRINGS ${EMULATOR_OUTPUT_TYPES})
list(FIND EMULATOR_OUTPUT_TYPES ${EMULATOR_OUTPUT_TYPE} INDEX)
if(INDEX EQUAL -1)
    message(FATAL_ERROR "Emulator output type ${EMULATOR_OUTPUT_TYPE} is not valid.")
else()
    message(STATUS "Setting emulator output type to ${EMULATOR_OUTPUT_TYPE}.")
endif()

# The selected video backend needs a matching QEMU display device. This code
# runs after add_subdirectory(kernel), where VIDEO_TYPE is defined.
if(NOT VIDEO_TYPE)
    message(FATAL_ERROR "VIDEO_TYPE is not set: the emulator configuration must come after add_subdirectory(kernel).")
endif()
if(VIDEO_TYPE STREQUAL "VIRTIO_GPU")
    set(EMULATOR_VGA_ARGS -vga virtio)
    set(EMULATOR_REQUIRE_BACKEND "virtio-gpu-2d")
else()
    set(EMULATOR_VGA_ARGS -vga std)
    set(EMULATOR_REQUIRE_BACKEND "")
endif()
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} ${EMULATOR_VGA_ARGS})
string(REPLACE ";" " " EMULATOR_VGA_STRING "${EMULATOR_VGA_ARGS}")
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -m 1096M)
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -rtc base=localtime)
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -nodefaults)
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -no-reboot)
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial file:${CMAKE_BINARY_DIR}/serial.log)
elseif(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_STDIO)
    set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -serial stdio)
endif()
set(EMULATOR_FLAGS ${EMULATOR_FLAGS} -drive file=${CMAKE_BINARY_DIR}/rootfs.img,format=raw,if=ide,index=0,media=disk)

# Boot the same GRUB image used by the ISO and test targets.
add_custom_target(
    qemu
    COMMAND test -e ${CMAKE_BINARY_DIR}/rootfs.img || ${CMAKE_COMMAND} -E cmake_echo_color --red "No filesystem file detected, you need to run: make filesystem"
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -boot d -cdrom ${CMAKE_BINARY_DIR}/cdrom.iso
    DEPENDS cdrom.iso
)

# Generate a GDB file containing symbols for all kernel, bootloader, and
# userspace executables.
add_custom_target(
    gdbinit
    BYPRODUCTS ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "add-symbol-file ${CMAKE_BINARY_DIR}/mentos/kernel.bin" > ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "add-symbol-file ${CMAKE_BINARY_DIR}/mentos/bootloader.bin" >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND find ${CMAKE_SOURCE_DIR}/filesystem/bin -type f | xargs realpath | sed 's/^/add-symbol-file /' >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "break boot.c: boot_main" >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "break kernel.c: kmain" >> ${CMAKE_BINARY_DIR}/gdb.run
    COMMAND echo "target remote localhost:1234" >> ${CMAKE_BINARY_DIR}/gdb.run
    DEPENDS ${CMAKE_BINARY_DIR}/mentos/bootloader.bin
    DEPENDS ${CMAKE_BINARY_DIR}/mentos/kernel.bin
    DEPENDS programs
    DEPENDS tests
    DEPENDS libc
)

# Boot QEMU paused for a remote debugger connection.
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

# Boot the GRUB image directly through QEMU.
add_custom_target(
    qemu-grub
    COMMAND ${EMULATOR} ${EMULATOR_FLAGS} -boot d -cdrom ${CMAKE_BINARY_DIR}/cdrom.iso
    DEPENDS cdrom.iso
)

# Run the userspace suite and validate its TAP output.
add_custom_target(
    qemu-test
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-test ${CMAKE_BINARY_DIR} 600 "${EMULATOR_VGA_STRING}" "${EMULATOR}" "${EMULATOR_REQUIRE_BACKEND}"
    DEPENDS cdrom_test.iso
)

# Run the kernel unit-test image. The short timeout treats a still-running
# guest as a hang rather than a slow test.
add_custom_target(
    qemu-kernel-test
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/run-qemu-kernel-test ${CMAKE_BINARY_DIR} 120 "${EMULATOR_VGA_STRING}" "${EMULATOR}"
    DEPENDS cdrom_kerneltest.iso
)
