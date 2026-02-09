# =============================================================================
# Deadlock Prevention Exercise - Library Layer
# =============================================================================

# Add library-side deadlock prevention sources
target_sources(libc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arr_math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smart_sem_user.c
)

# Add include directories
target_include_directories(libc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)
