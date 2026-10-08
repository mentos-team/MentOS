# -----------------------------------------------------------------------------
# DOCUMENTATION
# -----------------------------------------------------------------------------

if(DOXYGEN_FOUND)
    FetchContent_Declare(doxygenawesome
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css
        GIT_TAG v2.4.2
    )
    FetchContent_MakeAvailable(doxygenawesome)

    mark_as_advanced(FORCE
        FETCHCONTENT_UPDATES_DISCONNECTED_DOXYGENAWESOME
        FETCHCONTENT_SOURCE_DIR_DOXYGENAWESOME
    )

    file(READ ${PROJECT_SOURCE_DIR}/kernel/inc/version.h version_file)
    string(REGEX MATCH "OS_MAJOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MAJOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MINOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MINOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MICRO_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MICRO_VERSION ${CMAKE_MATCH_1})

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

    set(DOXYGEN_HTML_HEADER ${doxygenawesome_SOURCE_DIR}/doxygen-custom/header.html)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET ${doxygenawesome_SOURCE_DIR}/doxygen-awesome.css)
    set(DOXYGEN_HTML_EXTRA_FILES
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-fragment-copy-button.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-paragraph-link.js
        ${doxygenawesome_SOURCE_DIR}/doxygen-awesome-darkmode-toggle.js
    )

    set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
    set(DOXYGEN_WARN_IF_DOC_ERROR YES)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)
    set(DOXYGEN_WARN_AS_ERROR NO CACHE STRING "Treat Doxygen warnings as errors")
    set_property(CACHE DOXYGEN_WARN_AS_ERROR PROPERTY STRINGS YES NO)

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
endif()
