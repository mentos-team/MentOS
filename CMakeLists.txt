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

# -----------------------------------------------------------------------------
# DOCUMENTATION
# -----------------------------------------------------------------------------

if(DOXYGEN_FOUND)

    # FetchContent: Doxygen Awesome CSS
    FetchContent_Declare(doxygenawesome
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
        GIT_TAG v2.4.2
    )
    FetchContent_MakeAvailable(doxygenawesome)

    # Hide FetchContent variables to avoid clutter in ccmake.
    mark_as_advanced(FORCE
        FETCHCONTENT_UPDATES_DISCONNECTED_DOXYGENAWESOME
        FETCHCONTENT_SOURCE_DIR_DOXYGENAWESOME
    )

    # Read the file with the version.
    file(READ ${PROJECT_SOURCE_DIR}/kernel/inc/version.h version_file)
    # Extract the version.
    string(REGEX MATCH "OS_MAJOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MAJOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MINOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MINOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MICRO_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MICRO_VERSION ${CMAKE_MATCH_1})

    # Customization: Doxygen Configuration
    set(DOXYGEN_WARN_FORMAT "$file:$line:1: $text")
    set(DOXYGEN_PROJECT_NAME "MentOS")
    set(DOXYGEN_PROJECT_BRIEF "The Mentoring Operating System")
    set(DOXYGEN_PROJECT_NUMBER "${OS_MAJOR_VERSION}.${OS_MINOR_VERSION}.${OS_MICRO_VERSION}")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${PROJECT_SOURCE_DIR}/README.md)
    set(DOXYGEN_SHOW_INCLUDE_FILES NO)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_GENERATE_LATEX NO)
    set(DOXYGEN_GENERATE_MAN NO)
    set(DOXYGEN_ENABLE_PREPROCESSING YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_MACRO_EXPANSION YES)
    set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
    set(DOXYGEN_PREDEFINED "__attribute__((x))= _syscall0= _syscall0(x)= _syscall1(x)= _syscall2(x)= _syscall3(x)=")

    # Styling and UX enhancements using Doxygen Awesome
    set(DOXYGEN_HTML_HEADER ${doxygenawesome_SOURCE_DIR}/doxygen-custom/header.html)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET ${doxygenawesome_SOURCE_DIR}/doxygen-awesome.css)
    set(DOXYGEN_HTML_EXTRA_FILES
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-fragment-copy-button.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-paragraph-link.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-darkmode-toggle.js
    )

    # Set stricter warnings for better documentation quality
    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)
    set(DOXYGEN_WARN_AS_ERROR NO CACHE STRING "Treat Doxygen warnings as errors")
    set_property(CACHE DOXYGEN_WARN_AS_ERROR PROPERTY STRINGS YES NO)

    # Add Doxygen documentation target.
    file(GLOB_RECURSE ALL_PROJECT_FILES
        "${PROJECT_SOURCE_DIR}/lib/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/lib/inc/*.h"
        "${PROJECT_SOURCE_DIR}/lib/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/lib/src/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/inc/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/src/*.c"
    )

    doxygen_add_docs(
        ${PROJECT_NAME}_documentation
        ${PROJECT_SOURCE_DIR}/README.md
        ${PROJECT_SOURCE_DIR}/LICENSE.md
        ${ALL_PROJECT_FILES}
        COMMENT "Generating Doxygen documentation"
    )
endif(DOXYGEN_FOUND)
