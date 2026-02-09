# =============================================================================
# PEDAGOGICAL EXAMPLES
# =============================================================================
# These are simplified, well-documented examples to help students learn MentOS
# concepts in a natural progression:
# - 01_hello.c:  Hello World
# - 02_fork.c:   Process creation
# - 03_pipes.c:  Inter-process communication
# - 04_file_io.c: File operations

# List of example programs.
set(EXAMPLE_LIST
    01_hello.c
    02_fork.c
    03_pipes.c
    04_file_io.c
)

# Set the directory where the compiled binaries will be placed.
set(MENTOS_EXAMPLES_DIR ${CMAKE_SOURCE_DIR}/filesystem/bin/examples)

foreach(FILE_NAME ${EXAMPLE_LIST})
    # =========================================================================
    # TARGET NAMING
    # =========================================================================
    # Prepare the program name.
    string(REPLACE ".c" "" EXECUTABLE_NAME ${FILE_NAME})
    # Set the name of the target.
    set(TARGET_NAME example_${EXECUTABLE_NAME})

    # =========================================================================
    # TEXT ADDRESS
    # =========================================================================
    # Randomize .text section address so when debugging symbols don't clash.
    # The allowed range is from 256MB to 2.75GB
    # Minimum allowed address: 0x10000000
    # Max allowed address: 0xB0000000
    string(MD5 RAND_HASH ${FILE_NAME})
    string(SUBSTRING ${RAND_HASH} 1 3 TEXADDR_INFIX)
    string(RANDOM LENGTH 1 ALPHABET 0123456789AB RANDOM_SEED ${RAND_HASH} TEXADDR_FIRST)
    set(TEXT_ADDR 0x${TEXADDR_FIRST}${TEXADDR_INFIX}0000)

    # =========================================================================
    # EXECUTABLE
    # =========================================================================
    # Create the target.
    add_executable(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE_NAME})
    # Add the dependency to libc.
    add_dependencies(${TARGET_NAME} libc)
    # Add the includes.
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/inc)
    # Link the libc library.
    target_link_libraries(${TARGET_NAME} libc)
    # We need to specify the name of the entry function.
    target_compile_options(${TARGET_NAME} PRIVATE -u_start)
    # Add the linking properties.
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-Wl,-Ttext=${TEXT_ADDR},-e_start,-melf_i386")
    # Set the output directory.
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MENTOS_EXAMPLES_DIR}")
    # Set the output name.
    set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${EXECUTABLE_NAME}")

    # Append the program name to the list of all the executables.
    list(APPEND ALL_EXAMPLES ${TARGET_NAME})
endforeach()

# Add the overall target that builds all the examples.
add_custom_target(examples ALL DEPENDS ${ALL_EXAMPLES})
