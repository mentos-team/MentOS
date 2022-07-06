# Find Doxygen
find_package(Doxygen)

if (DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/html/index.html)

    # Replace variables inside @@ with the current values.
    set(DOXYFILE_IN  ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

    # Copy the files needd by the documentation.
    set(DOXYFILE_CSS    ${CMAKE_CURRENT_SOURCE_DIR}/doxygen.css)
    set(DOXYFILE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/header.html)
    set(DOXYFILE_FOOTER ${CMAKE_CURRENT_SOURCE_DIR}/footer.html)

    # Doxygen won't create this for us.
    file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR})

    add_custom_command(
            OUTPUT ${DOXYGEN_INDEX_FILE}
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}

            MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
            COMMENT "Generating docs"
    )
    add_custom_target(
            doxygen_doc
            DEPENDS
            ${DOXYGEN_INDEX_FILE}
            ${DOXYFILE_IN}
            ${DOXYFILE_CSS}
            ${DOXYFILE_HEADER}
            ${DOXYFILE_FOOTER}
    )
endif (DOXYGEN_FOUND)
