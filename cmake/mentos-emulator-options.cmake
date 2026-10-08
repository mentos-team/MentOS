# =============================================================================
# EMULATOR OUTPUT OPTION
# =============================================================================

# This option is consumed by kernel/CMakeLists.txt while the kernel target is
# configured, so it must be defined before the project subdirectories are
# added. The emulator target module consumes the same validated value later.
set(EMULATOR_OUTPUT_TYPES OUTPUT_STDIO OUTPUT_LOG)
set(EMULATOR_OUTPUT_TYPE "OUTPUT_STDIO" CACHE STRING "Choose the emulator output type: ${EMULATOR_OUTPUT_TYPES}")
set_property(CACHE EMULATOR_OUTPUT_TYPE PROPERTY STRINGS ${EMULATOR_OUTPUT_TYPES})
list(FIND EMULATOR_OUTPUT_TYPES ${EMULATOR_OUTPUT_TYPE} EMULATOR_OUTPUT_TYPE_INDEX)
if(EMULATOR_OUTPUT_TYPE_INDEX EQUAL -1)
    message(FATAL_ERROR "Emulator output type ${EMULATOR_OUTPUT_TYPE} is not valid.")
else()
    message(STATUS "Setting emulator output type to ${EMULATOR_OUTPUT_TYPE}.")
endif()
