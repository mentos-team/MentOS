# =============================================================================
# Author: Enrico Fraccaroli
# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)
# Initialize the project.
project(tests C)

# =============================================================================
# C WARNINGs
# =============================================================================

# We need to specify the name of the entry function.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -u_start")

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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-command-line-argument")

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
# BUILD
# =============================================================================

# Set the directory where the compiled binaries will be placed.
set(MENTOS_TESTS_DIR ${CMAKE_SOURCE_DIR}/files/bin/tests)

# Add the executables (manually).
set(TESTS
    t_mem.c
    t_fork.c
    # Real-time programs
    t_periodic1.c
    t_periodic2.c
    t_periodic3.c
    # Exec
    t_exec.c
    t_exec_callee.c
    # Test environmental variables.
    t_setenv.c
    t_getenv.c
    # Test: signals.
    t_stopcont.c
    t_sigusr.c
    t_abort.c
    t_sigfpe.c
    t_siginfo.c
    t_sleep.c
    t_sigaction.c
    t_sigmask.c
    t_groups.c
    t_alarm.c
    t_kill.c
    t_itimer.c
    t_gid.c
)

foreach(TEST ${TESTS})
    # Prepare the program name.
    string(REPLACE ".c" "" TEST_NAME ${TEST})

    # Set the name of the target.
    set(TARGET_NAME test_${TEST_NAME})

    # Randomize .text section address so when debugging symbols don't clash.
    # The allowed range is from 256MB to 2.75GB
    # Minimum allowed address: 0x10000000
    # Max allowed address: 0xB0000000
    string(MD5 RAND_HASH ${TEST})
    string(SUBSTRING ${RAND_HASH} 1 3 TEXADDR_INFIX)
    string(RANDOM LENGTH 1 ALPHABET 0123456789AB RANDOM_SEED ${RAND_HASH} TEXADDR_FIRST)

    # Create the target.
    add_executable(${TARGET_NAME} ${TEST})

    # Add the includes.
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libc/inc)

    # Link the libc library.
    target_link_libraries(${TARGET_NAME} ${CMAKE_BINARY_DIR}/libc/libc.a)

    # Add the dependency to libc.
    add_dependencies(${TARGET_NAME} libc)

    # Add the linking properties.
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-Wl,-Ttext=0x${TEXADDR_FIRST}${TEXADDR_INFIX}0000,-e_start")

    # Add the final target that strips the debugging symbols from the program.
    add_custom_command(TARGET ${TARGET_NAME}
        BYPRODUCTS ${MENTOS_TESTS_DIR}/${TEST_NAME}
        COMMAND mkdir -p  ${MENTOS_TESTS_DIR}
        COMMAND strip --strip-debug ${PROJECT_BINARY_DIR}/${TARGET_NAME} -o ${MENTOS_TESTS_DIR}/${TEST_NAME}
        DEPENDS ${TARGET_NAME}
    )

    # Log the entry.
    message(VERBOSE "Test ${TEST_NAME}, `.text` at 0x${TEXADDR_FIRST}${TEXADDR_INFIX}0000")

    # Append the program name to the list of all the executables.
    list(APPEND ALL_TESTS ${TEST_NAME})
endforeach()

# Add the overall target that builds all the programs.
add_custom_target(all_tests ALL DEPENDS ${ALL_TESTS})
