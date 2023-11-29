# =============================================================================
# Author: Enrico Fraccaroli
# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)
# Initialize the project.
project(tests C)

if((${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Darwin") OR APPLE)
    set(CMAKE_C_COMPILER x86_64-elf-gcc)
    set(CMAKE_CXX_COMPILER x86_64-elf-g++)
endif()

# =============================================================================
# List of programs.
# =============================================================================
# Add your file in this list.
set(TESTS
    t_mem.c
    t_fork.c
    # Test real-time programs.
    t_periodic1.c
    t_periodic2.c
    t_periodic3.c
    # Testing task execution.
    t_exec.c
    t_exec_callee.c
    # Test environmental variables.
    t_setenv.c
    t_getenv.c
    # Test signals.
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
    # Test semaphores.
    t_semget.c
    t_semflg.c
    t_semop.c
    # Test message queues.
    t_msgget.c
    # Test shared memory.
    t_shmget.c
    t_shm_read.c
    t_shm_write.c
    # Test scheduling feedback tests.
    t_schedfb.c
)

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

foreach(TEST ${TESTS})
    # Prepare the program name.
    string(REPLACE ".c" "" TEST_NAME ${TEST})
    # Set the name of the target.
    set(TARGET_NAME test_${TEST_NAME})
    # Create the target.
    add_executable(${TARGET_NAME} ${TEST})
    # Add the includes.
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libc/inc)
    # Link the libc library.
    target_link_libraries(${TARGET_NAME} ${CMAKE_BINARY_DIR}/libc/libc.a)
    # Add the dependency to libc.
    add_dependencies(${TARGET_NAME} libc)
    # Copy the progam to the bin folder.
    add_custom_command(
        TARGET ${TARGET_NAME}
        BYPRODUCTS ${MENTOS_TESTS_DIR}/${TEST_NAME}
        COMMAND mkdir -p ${MENTOS_TESTS_DIR}
        COMMAND cp ${PROJECT_BINARY_DIR}/${TARGET_NAME} ${MENTOS_TESTS_DIR}/${TEST_NAME}
        DEPENDS ${TARGET_NAME}
    )
    # Append the program name to the list of all the executables.
    list(APPEND ALL_TESTS ${TARGET_NAME})
endforeach()

# Add the overall target that builds all the programs.
add_custom_target(all_tests ALL DEPENDS ${ALL_TESTS})
