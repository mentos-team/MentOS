# ------------------------------------------------------------------------------
# Initialize the project.
project(initfscp C ASM)

# ------------------------------------------------------------------------------
# Set the project name.
set(PROJECT_NAME initfscp)

# ------------------------------------------------------------------------------
# Set the compiler options (original).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")

# ------------------------------------------------------------------------------
# Add the includes.
include_directories(inc)

# ------------------------------------------------------------------------------
# Add the source files.
set(SOURCE_FILES src/initfscp.c)

# ------------------------------------------------------------------------------
# Add the executable.
add_executable(initfscp ${SOURCE_FILES})


