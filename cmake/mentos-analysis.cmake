# -----------------------------------------------------------------------------
# CODE ANALYSIS
# -----------------------------------------------------------------------------

if(CLANG_TIDY_EXE)
    file(GLOB_RECURSE ALL_PROJECT_FILES
        "${PROJECT_SOURCE_DIR}/lib/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/lib/inc/*.h"
        "${PROJECT_SOURCE_DIR}/lib/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/lib/src/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/inc/**/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/inc/*.h"
        "${PROJECT_SOURCE_DIR}/kernel/src/**/*.c"
        "${PROJECT_SOURCE_DIR}/kernel/src/*.c"
        "${PROJECT_SOURCE_DIR}/userspace/**/*.h"
        "${PROJECT_SOURCE_DIR}/userspace/*.h"
        "${PROJECT_SOURCE_DIR}/userspace/**/*.c"
        "${PROJECT_SOURCE_DIR}/userspace/*.c"
    )
    add_custom_target(
        ${PROJECT_NAME}_clang_tidy
        COMMAND ${CLANG_TIDY_EXE}
        --system-headers=0
        --p=${CMAKE_BINARY_DIR}
        ${ALL_PROJECT_FILES}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
    add_custom_target(
        ${PROJECT_NAME}_clang_tidy_fix
        COMMAND ${CLANG_TIDY_EXE}
        --system-headers=0
        --fix --fix-errors
        --p=${CMAKE_BINARY_DIR}
        ${ALL_PROJECT_FILES}
        COMMENT "Running clang-tidy-fix"
        VERBATIM
    )
endif()
