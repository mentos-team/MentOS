# ------------------------------------------------------------------------------
# Initialize the project.
project(initfscp C)

# ------------------------------------------------------------------------------
# Set the project name.
set(PROJECT_NAME initfscp)

if (CMAKE_CROSSCOMPILING)
    set(CMAKE_C_COMPILER gcc)
    set(CMAKE_CXX_COMPILER g++)
    set(CMAKE_EXE_LINKER_FLAGS "")
endif (CMAKE_CROSSCOMPILING)

# ------------------------------------------------------------------------------
# Set the compiler options (original).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# ------------------------------------------------------------------------------
# Add the source files.
set(SOURCE_FILES src/initfscp.c)

# ------------------------------------------------------------------------------
# Add the executable.
add_executable(initfscp ${SOURCE_FILES})


