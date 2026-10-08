# =============================================================================
# TARGET-SCOPED COMPILER AND LINKER OPTIONS
# =============================================================================

# Keep warnings and build-mode options on a target rather than on the global
# CMAKE_C_FLAGS variable. This prevents future host-side tools from inheriting
# the freestanding kernel ABI by accident.
add_library(mentos_common_options INTERFACE)

set(_mentos_warning_options
    -Wall
    -Werror
    -Wpedantic
    -pedantic-errors
    -Wshadow
    -std=gnu99
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unknown-pragmas
    -Wno-missing-braces
)

foreach(_option IN LISTS _mentos_warning_options)
    target_compile_options(mentos_common_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:${_option}>")
endforeach()

# This option is understood by Clang but not GCC. Keeping it compiler-scoped
# avoids the noisy GCC diagnostic that was previously emitted in every build.
target_compile_options(mentos_common_options INTERFACE
    "$<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:Clang>>:-Wno-unused-command-line-argument>")

if(FORCE_DEBUG_LOGLEVEL)
    target_compile_definitions(mentos_common_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:MENTOS_FORCE_DEBUG_LOGLEVEL>")
endif()

if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 10)
    target_compile_options(mentos_common_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:-fcommon>")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(mentos_common_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:-g3;-ggdb;-O0>")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(mentos_common_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:-O2>")
endif()

# Freestanding C compilation is deliberately separate from the linker
# contract. `target_link_options` keeps -nostdlib and -static off compile
# commands while still propagating them to userspace executables through libc.
add_library(mentos_freestanding_options INTERFACE)
target_link_libraries(mentos_freestanding_options INTERFACE mentos_common_options)

foreach(_option
    -nostdinc
    -fno-builtin
    -fno-stack-protector
    -fno-pic
    -fomit-frame-pointer
    -m32
    -march=i686
)
    target_compile_options(mentos_freestanding_options INTERFACE
        "$<$<COMPILE_LANGUAGE:C>:${_option}>")
endforeach()

target_link_options(mentos_freestanding_options INTERFACE
    -static
    -nostdlib
    -m32
)
