# =============================================================================
# EXTERNAL TOOL DISCOVERY
# =============================================================================

# Optional developer tools do not prevent compiler-only configurations from
# being generated. Targets that need a missing tool fail when invoked, with a
# diagnostic emitted here during configuration.
find_program(CLANG_TIDY_EXE NAMES clang-tidy)

# Find the NASM compiler used by the boot and kernel assembly sources.
find_program(ASM_COMPILER NAMES nasm HINTS /usr/bin/ /usr/local/bin/)
mark_as_advanced(ASM_COMPILER)
if(NOT ASM_COMPILER)
    message(FATAL_ERROR "ASM compiler not found!")
endif()
set(CMAKE_ASM_COMPILER ${ASM_COMPILER})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> -f elf -g -O0 -F dwarf -o <OBJECT> <SOURCE>")
else()
    set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> -f elf -g -O2 -o <OBJECT> <SOURCE>")
endif()

# mke2fs lives in sbin, which is not on every user's PATH, and on macOS
# Homebrew keeps e2fsprogs keg-only so nothing of it is linked. Look it up
# by name and say so when it is missing, instead of failing inside the
# filesystem target with a bare "command not found" (#391).
find_program(
    MKE2FS_EXE
    NAMES mke2fs
    HINTS /usr/sbin /sbin /usr/local/sbin
          /opt/homebrew/opt/e2fsprogs/sbin
          /usr/local/opt/e2fsprogs/sbin
)
if(NOT MKE2FS_EXE)
    message(WARNING
        "mke2fs not found: the `filesystem` target will not work. It ships with "
        "e2fsprogs: `apt install e2fsprogs`, or on macOS `brew install e2fsprogs` "
        "and add $(brew --prefix e2fsprogs)/sbin to PATH.")
    set(MKE2FS_EXE mke2fs)
endif()

# GRUB is only required when an ISO target is built. Keep it optional at
# configure time so compiler-only builds remain usable on hosts without the
# GRUB utilities.
find_program(GRUB_MKRESCUE_EXE NAMES grub-mkrescue)
if(NOT GRUB_MKRESCUE_EXE)
    message(WARNING
        "grub-mkrescue not found: ISO targets will not work. It ships with "
        "GRUB utilities (for example, `apt install grub-pc-bin xorriso`).")
    set(GRUB_MKRESCUE_EXE grub-mkrescue)
endif()

# QEMU is only required when an emulator target is built. The fallback keeps
# direct compiler and packaging targets usable on hosts without an emulator.
find_program(EMULATOR NAMES qemu-system-i386)
if(NOT EMULATOR)
    message(WARNING
        "qemu-system-i386 not found: QEMU targets will not work until it is "
        "installed and available on PATH.")
    set(EMULATOR qemu-system-i386)
endif()

mark_as_advanced(GRUB_MKRESCUE_EXE EMULATOR)
