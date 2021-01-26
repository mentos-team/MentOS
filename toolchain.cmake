set(CMAKE_SYSTEM_NAME Generic)

if(CMAKE_VERSION VERSION_LESS "3.6.0")
    INCLUDE(CMakeForceCompiler)
    CMAKE_FORCE_C_COMPILER(i386-elf-gcc Clang)
    CMAKE_FORCE_CXX_COMPILER(i386-elf-g++ Clang)
else()
    set(CMAKE_C_COMPILER i386-elf-gcc)
    set(CMAKE_CXX_COMPILER i386-elf-g++)
    set(CMAKE_AR i386-elf-ar)
endif()

set(CMAKE_LINKER i386-elf-ld)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")
