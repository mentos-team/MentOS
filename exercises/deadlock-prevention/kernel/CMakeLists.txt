# =============================================================================
# Deadlock Prevention Exercise - Kernel Layer
# =============================================================================

# Add kernel-side deadlock prevention sources
target_sources(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deadlock_prevention.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/deadlock_simulation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/resource.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/smart_sem_kernel.c
)

# Add include directories
target_include_directories(kernel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/exercises/deadlock-prevention/lib/inc
)
