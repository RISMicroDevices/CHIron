if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "Debug")
    return()
endif()

if(NOT DEFINED NM_EXECUTABLE OR NM_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "NM_EXECUTABLE is required.")
endif()

if(NOT DEFINED INPUT_FILE OR INPUT_FILE STREQUAL "")
    message(FATAL_ERROR "INPUT_FILE is required.")
endif()

if(NOT DEFINED OUTPUT_FILE OR OUTPUT_FILE STREQUAL "")
    message(FATAL_ERROR "OUTPUT_FILE is required.")
endif()

execute_process(
    COMMAND "${NM_EXECUTABLE}" -C -n "${INPUT_FILE}"
    OUTPUT_FILE "${OUTPUT_FILE}"
    RESULT_VARIABLE nm_result
)

if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Failed to export symbols with nm. Exit code: ${nm_result}")
endif()
