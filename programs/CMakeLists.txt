# =============================================================================
# Author: Enrico Fraccaroli
# =============================================================================
# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)
# Initialize the project.
project(programs C)

# =============================================================================
# List of programs.
# =============================================================================
# Add your file in this list.
set(PROGRAMS
    init.c
    cpuid.c
    clear.c
    date.c
    ls.c
    logo.c
    nice.c
    poweroff.c
    rm.c
    shell.c
    uname.c
    cat.c
    echo.c
    login.c
    mkdir.c
    pwd.c
    rmdir.c
    showpid.c
    touch.c
    env.c
    ps.c
    kill.c
    ipcrm.c
    ipcs.c
    sleep.c
    man.c
    edit.c
    uptime.c
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
set(MENTOS_BIN_DIR ${CMAKE_SOURCE_DIR}/files/bin)

foreach(PROGRAM ${PROGRAMS})
    # Prepare the program name.
    string(REPLACE ".c" "" PROGRAM_NAME ${PROGRAM})
    # Set the name of the target.
    set(TARGET_NAME prog_${PROGRAM_NAME})
    # Create the target.
    add_executable(${TARGET_NAME} ${PROGRAM})
    # Add the includes.
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libc/inc)
    # Link the libc library.
    target_link_libraries(${TARGET_NAME} ${CMAKE_BINARY_DIR}/libc/libc.a)
    # Add the dependency to libc.
    add_dependencies(${TARGET_NAME} libc)
    # Copy the progam to the bin folder.
    add_custom_command(
        TARGET ${TARGET_NAME}
        BYPRODUCTS ${MENTOS_BIN_DIR}/${PROGRAM_NAME}
        COMMAND mkdir -p ${MENTOS_BIN_DIR}
        COMMAND cp ${PROJECT_BINARY_DIR}/${TARGET_NAME} ${MENTOS_BIN_DIR}/${PROGRAM_NAME}
        DEPENDS ${TARGET_NAME}
    )
    # Append the program name to the list of all the executables.
    list(APPEND ALL_EXECUTABLES ${TARGET_NAME})
endforeach()

# Add the overall target that builds all the programs.
add_custom_target(all_programs ALL DEPENDS ${ALL_EXECUTABLES})
