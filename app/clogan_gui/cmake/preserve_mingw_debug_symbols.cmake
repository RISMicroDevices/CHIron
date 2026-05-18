if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "Debug")
    return()
endif()

if(NOT DEFINED OBJCOPY_EXECUTABLE OR OBJCOPY_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "OBJCOPY_EXECUTABLE is required.")
endif()

if(NOT DEFINED INPUT_FILE OR INPUT_FILE STREQUAL "")
    message(FATAL_ERROR "INPUT_FILE is required.")
endif()

if(NOT DEFINED OUTPUT_FILE OR OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "OUTPUT_FILE is required.")
endif()

execute_process(
    COMMAND "${OBJCOPY_EXECUTABLE}" --only-keep-debug "${INPUT_FILE}" "${OUTPUT_FILE}"
    RESULT_VARIABLE objcopy_result
)

if(NOT objcopy_result EQUAL 0)
    message(FATAL_ERROR "Failed to preserve debug symbols with objcopy. Exit code: ${objcopy_result}")
endif()
