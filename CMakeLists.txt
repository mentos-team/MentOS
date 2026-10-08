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
include(${CMAKE_SOURCE_DIR}/cmake/mentos-emulator-options.cmake)

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
include(${CMAKE_SOURCE_DIR}/cmake/mentos-iso.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/mentos-emulator.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/mentos-analysis.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/mentos-documentation.cmake)
