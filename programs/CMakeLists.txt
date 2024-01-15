# =============================================================================
# Author: Enrico Fraccaroli
# =============================================================================

# Set the minimum required version of cmake.
cmake_minimum_required(VERSION 3.1...3.22)

# Initialize the project.
project(programs C)

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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=i686")
if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcommon")
endif()

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
# Add the executables (manually).
set(PROGRAMS
    nice.c
    ls.c
    mkdir.c
    ipcrm.c
    login.c
    man.c
    logo.c
    shell.c
    cpuid.c
    clear.c
    rmdir.c
    poweroff.c
    rm.c
    cat.c
    init.c
    ps.c
    kill.c
    edit.c
    sleep.c
    date.c
    echo.c
    showpid.c
    uname.c
    ipcs.c
    touch.c
    uptime.c
    pwd.c
    env.c
)
# Set the directory where the compiled binaries will be placed.
set(MENTOS_BIN_DIR ${CMAKE_SOURCE_DIR}/files/bin)

foreach(FILE_NAME ${PROGRAMS})
    # Prepare the program name.
    string(REPLACE ".c" "" EXECUTABLE_NAME ${FILE_NAME})

    # Set the name of the target.
    set(TARGET_NAME prog_${EXECUTABLE_NAME})

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
    add_executable(${TARGET_NAME} ${CMAKE_SOURCE_DIR}/programs/${FILE_NAME})
    # Add the dependency to libc.
    add_dependencies(${TARGET_NAME} libc)
    # Add the includes.
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libc/inc)
    # Link the libc library.
    target_link_libraries(${TARGET_NAME} libc)
    # We need to specify the name of the entry function.
    target_compile_options(${TARGET_NAME} PRIVATE -u_start)
    # Add the linking properties.
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-Wl,-Ttext=${TEXT_ADDR},-e_start,-melf_i386")
    # Set the output directory.
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MENTOS_BIN_DIR}")
    # Set the output name.
    set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${EXECUTABLE_NAME}")

    # Append the program name to the list of all the executables.
    list(APPEND ALL_EXECUTABLES ${TARGET_NAME})
endforeach()

# Add the overall target that builds all the programs.
add_custom_target(all_programs ALL DEPENDS ${ALL_EXECUTABLES})
