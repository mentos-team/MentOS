# =============================================================================
# LIBRARY
# =============================================================================

# Automatically collect all library source files.
file(GLOB_RECURSE LIBC_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S"
)

# Create the libc library.
add_library(libc ${LIBC_SOURCES})

# Add the includes.
target_include_directories(libc PUBLIC inc)

# Remove the 'lib' prefix.
set_target_properties(libc PROPERTIES PREFIX "")

# If the emulator is set to output on a log file, tell that to the C code.
if(${EMULATOR_OUTPUT_TYPE} STREQUAL OUTPUT_LOG)
    target_compile_definitions(libc PUBLIC EMULATOR_OUTPUT_LOG)
endif()

target_compile_definitions(
    libc PUBLIC
    MENTOS_ROOT="${CMAKE_SOURCE_DIR}"
)
