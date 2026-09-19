# Plants the deep-path symlink fixture that t_symlink walks (#288) into
# the freshly generated rootfs image.
#
# The image ships a chain of real directories ending in a symlink whose
# target is long enough that substituting it at the end of an almost-full
# path buffer would write past the end of the buffer. The chain cannot be
# staged through the `filesystem/` tree: a guest path of PATH_MAX
# characters plus any checkout prefix does not fit the host PATH_MAX, so
# the fixture is created inside the image with debugfs after mke2fs has
# packed it.
#
# The guest path
#
#     /.t_symlink_deep/<dir...>/<dir...>/ln
#
# must be exactly `PATH_MAX - 1` characters long, so that appending the
# link name still passes the append guard of `__resolve_path`, and the
# link substitution itself (`dst + linklen >= PATH_MAX`) is what has to
# reject it with -ENAMETOOLONG.
#
# Every component stays at or below 254 characters (tokenize rejects a
# component of NAME_MAX = 255), and the link target is 59 characters, so
# ext2 stores it inline, below the 60-byte inline field.
#
# Run by the `filesystem` target:
#     cmake -DROOTFS=<image> -DBINARY_DIR=<build> -P scripts/make-symlink-fixture.cmake

if(NOT DEFINED ROOTFS OR NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "symlink fixture: ROOTFS and BINARY_DIR must be defined")
endif()

# debugfs lives in sbin, which is not on every user's PATH, and on macOS
# Homebrew keeps e2fsprogs keg-only so nothing of it is linked at all.
# Looking it up by hand lets the failure say which tool is missing instead
# of blaming the fixture (#391).
find_program(
    DEBUGFS_EXE
    NAMES debugfs
    HINTS /usr/sbin /sbin /usr/local/sbin
          /opt/homebrew/opt/e2fsprogs/sbin
          /usr/local/opt/e2fsprogs/sbin
)
if(NOT DEBUGFS_EXE)
    message(FATAL_ERROR
        "symlink fixture: debugfs not found. It ships with e2fsprogs: "
        "`apt install e2fsprogs`, or on macOS `brew install e2fsprogs` and add "
        "$(brew --prefix e2fsprogs)/sbin to PATH.")
endif()

# Must stay in sync with lib/inc/limits.h.
set(PATH_MAX 4096)
# Guest path of the fixture root, with the leading slash.
set(ROOT_PATH "/.t_symlink_deep")
# Name of the trailing symlink.
set(LINK_NAME "ln")
# Length of the link target: 59 'a' characters.
set(TARGET_LEN 59)

string(LENGTH "${ROOT_PATH}" prefix_len)
math(EXPR budget "${PATH_MAX} - 1 - ${prefix_len} - 3") # -3: the trailing "/ln"

# Fill the budget with components of the longest usable length, 254
# characters, which cost 255 bytes each with their separator; the last
# component takes what is left.
set(lengths "")
while(budget GREATER 255)
    math(EXPR budget "${budget} - 255")
    list(APPEND lengths 254)
endwhile()
if(budget LESS 1 OR budget GREATER 255)
    message(FATAL_ERROR "symlink fixture: leftover component budget ${budget} is not a usable length")
endif()
math(EXPR last "${budget} - 1")
list(APPEND lengths "${last}")

# Rebuild the expected length and check it before touching the image.
set(total "${prefix_len}")
foreach(len IN LISTS lengths)
    math(EXPR total "${total} + 1 + ${len}")
endforeach()
math(EXPR total "${total} + 3")
math(EXPR expected "${PATH_MAX} - 1")
if(NOT total EQUAL expected)
    message(FATAL_ERROR "symlink fixture: path length ${total} does not end at PATH_MAX - 1 = ${expected}")
endif()

# Build the debugfs command list.
#
# Every command names ONE component and then descends into it, instead of
# repeating the absolute path each time. debugfs reads its command file
# into a BUFSIZ-sized buffer, so a line longer than BUFSIZ is silently
# truncated and the command is lost. BUFSIZ is 8192 with glibc but 1024 on
# macOS, and the absolute-path form reached 4163 characters here, so the
# fixture was built on Linux and quietly not built on macOS (#391).
# Measured: with absolute paths the chain stops at the first line past
# BUFSIZ; with `cd` no line exceeds the length of one component.
set(path "${ROOT_PATH}")
set(names "")
set(commands "mkdir ${ROOT_PATH}\ncd ${ROOT_PATH}\n")
foreach(len IN LISTS lengths)
    set(name "")
    foreach(i RANGE 1 ${len})
        string(APPEND name "d")
    endforeach()
    list(APPEND names "${name}")
    set(path "${path}/${name}")
    string(APPEND commands "mkdir ${name}\ncd ${name}\n")
endforeach()
set(target "")
foreach(i RANGE 1 ${TARGET_LEN})
    string(APPEND target "a")
endforeach()
string(APPEND commands "symlink ${LINK_NAME} ${target}\n")

file(WRITE "${BINARY_DIR}/symlink-fixture.cmds" "${commands}")

# debugfs does not reliably fail the whole run when a single command
# fails, so the link is looked up again afterwards: the fixture counts as
# generated only if the full-length path resolves to a symlink. The lookup
# descends the same way, for the same reason.
execute_process(
    COMMAND "${DEBUGFS_EXE}" -w -f "${BINARY_DIR}/symlink-fixture.cmds" "${ROOTFS}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_output
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "symlink fixture: debugfs failed with ${build_result} on ${ROOTFS}.\n${build_output}")
endif()

set(check "cd ${ROOT_PATH}\n")
foreach(name IN LISTS names)
    string(APPEND check "cd ${name}\n")
endforeach()
string(APPEND check "stat ${LINK_NAME}\n")
file(WRITE "${BINARY_DIR}/symlink-fixture-check.cmds" "${check}")

execute_process(
    COMMAND "${DEBUGFS_EXE}" -f "${BINARY_DIR}/symlink-fixture-check.cmds" "${ROOTFS}"
    OUTPUT_VARIABLE stat_output
    ERROR_VARIABLE stat_output
)
if(NOT stat_output MATCHES "Type: *symlink")
    message(FATAL_ERROR
        "symlink fixture: the deep link was not created in ${ROOTFS}.\n${stat_output}")
endif()

message(STATUS "symlink fixture: ${total}-character path with trailing symlink planted in ${ROOTFS}")
