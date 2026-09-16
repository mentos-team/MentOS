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

# Build the debugfs command list: one mkdir per level, then the link.
set(path "${ROOT_PATH}")
set(commands "mkdir ${path}\n")
foreach(len IN LISTS lengths)
    set(name "")
    foreach(i RANGE 1 ${len})
        string(APPEND name "d")
    endforeach()
    set(path "${path}/${name}")
    string(APPEND commands "mkdir ${path}\n")
endforeach()
set(target "")
foreach(i RANGE 1 ${TARGET_LEN})
    string(APPEND target "a")
endforeach()
string(APPEND commands "symlink ${path}/${LINK_NAME} ${target}\n")

file(WRITE "${BINARY_DIR}/symlink-fixture.cmds" "${commands}")

# debugfs does not reliably fail the whole run when a single command
# fails, so the link is looked up again afterwards: the fixture counts as
# generated only if the full-length path resolves to a symlink.
execute_process(
    COMMAND debugfs -w -f "${BINARY_DIR}/symlink-fixture.cmds" "${ROOTFS}"
    OUTPUT_QUIET ERROR_QUIET
)
execute_process(
    COMMAND debugfs -R "stat ${path}/${LINK_NAME}" "${ROOTFS}"
    OUTPUT_VARIABLE stat_output
    ERROR_QUIET
)
if(NOT stat_output MATCHES "Type: *symlink")
    message(FATAL_ERROR "symlink fixture: the deep link was not created in ${ROOTFS}")
endif()

message(STATUS "symlink fixture: ${total}-character path with trailing symlink planted in ${ROOTFS}")
