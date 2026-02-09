# =============================================================================
# Deadlock Prevention - Userspace Simulations
# =============================================================================

# List of simulation programs
set(DEADLOCK_SIMULATIONS
    deadlock_simulation.c
)

foreach(FILE_NAME ${DEADLOCK_SIMULATIONS})
    # Prepare the program name
    string(REPLACE ".c" "" EXECUTABLE_NAME ${FILE_NAME})
    set(TARGET_NAME sim_deadlock_${EXECUTABLE_NAME})

    # Randomize .text section address
    string(MD5 RAND_HASH ${FILE_NAME})
    string(SUBSTRING ${RAND_HASH} 1 3 TEXADDR_INFIX)
    string(RANDOM LENGTH 1 ALPHABET 0123456789AB RANDOM_SEED ${RAND_HASH} TEXADDR_FIRST)
    set(TEXT_ADDR 0x${TEXADDR_FIRST}${TEXADDR_INFIX}0000)

    # Create the executable
    add_executable(${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE_NAME})
    add_dependencies(${TARGET_NAME} libc)
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib/inc)
    target_link_libraries(${TARGET_NAME} libc)
    target_compile_options(${TARGET_NAME} PRIVATE -u_start)
    set_target_properties(${TARGET_NAME} PROPERTIES 
        LINK_FLAGS "-Wl,-Ttext=${TEXT_ADDR},-e_start,-melf_i386"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/filesystem/bin/tests"
        OUTPUT_NAME "${EXECUTABLE_NAME}"
    )
endforeach()
