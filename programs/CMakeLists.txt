# List of programs.
set(PROGRAM_LIST
    cat.c
    chmod.c
    chown.c
    clear.c
    cp.c
    cpuid.c
    date.c
    echo.c
    edit.c
    env.c
    false.c
    head.c
    id.c
    init.c
    ipcrm.c
    ipcs.c
    kill.c
    login.c
    logo.c
    ls.c
    man.c
    mkdir.c
    more.c
    nice.c
    poweroff.c
    ps.c
    pwd.c
    reset.c
    rm.c
    rmdir.c
    runtests.c
    shell.c
    showpid.c
    sleep.c
    stat.c
    touch.c
    uname.c
    uptime.c
)

# Set the directory where the compiled binaries will be placed.
set(MENTOS_BIN_DIR ${CMAKE_SOURCE_DIR}/files/bin)

foreach(FILE_NAME ${PROGRAM_LIST})
    # =========================================================================
    # TARGET NAMING
    # =========================================================================
    # Prepare the program name.
    string(REPLACE ".c" "" EXECUTABLE_NAME ${FILE_NAME})
    # Set the name of the target.
    set(TARGET_NAME prog_${EXECUTABLE_NAME})

    # =========================================================================
    # TEXT ADDRESS
    # =========================================================================
    # Randomize .text section address so when debugging symbols don't clash.
    # User-space range: 0x08000000 to 0x0BFFFFFF (safe user space, no kernel overlap)
    # Third hex digit must be 8,9,A,B only (restrict from 0-F to 8-B)
    string(MD5 RAND_HASH ${FILE_NAME})
    string(SUBSTRING ${RAND_HASH} 1 2 TEXADDR_MID)
    string(RANDOM LENGTH 1 ALPHABET 89AB RANDOM_SEED ${RAND_HASH} TEXADDR_HIGH)
    # Create address in range 0x08XXXXXX to 0x0BXXXXXX (safe user space)
    set(TEXT_ADDR 0x0${TEXADDR_HIGH}${TEXADDR_MID}0000)
    if(FILE_NAME MATCHES "^init")
        message(STATUS "init: TEXT_ADDR=${TEXT_ADDR}")
    endif()

    # =========================================================================
    # EXECUTABLE
    # =========================================================================
    # Create the target.
    add_executable(${TARGET_NAME} ${CMAKE_SOURCE_DIR}/programs/${FILE_NAME})
    # Add the dependency to libc.
    add_dependencies(${TARGET_NAME} libc)
    # Add the includes.
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libc/inc)
    # Link the libc library.
    target_link_libraries(${TARGET_NAME} libc)
    # We need to specify the name of the entry function.
    target_compile_options(${TARGET_NAME} PRIVATE -u_start)
    # Add the linking properties.
    # Place .text.entry section first to ensure consistent entry point offset
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "-Wl,-Ttext=${TEXT_ADDR},-e_start,-melf_i386,--sort-section=alignment")
    # Set the output directory.
    set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${MENTOS_BIN_DIR}")
    # Set the output name.
    set_target_properties(${TARGET_NAME} PROPERTIES OUTPUT_NAME "${EXECUTABLE_NAME}")

    # Append the program name to the list of all the executables.
    list(APPEND ALL_EXECUTABLES ${TARGET_NAME})
endforeach()

# Add the overall target that builds all the programs.
add_custom_target(programs ALL DEPENDS ${ALL_EXECUTABLES})
