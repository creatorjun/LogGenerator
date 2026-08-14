# tests/cli_smoke.cmake
if(NOT DEFINED CLI_PATH OR NOT DEFINED SOURCE_ROOT OR NOT DEFINED BUILD_ROOT)
    message(FATAL_ERROR "CLI_PATH, SOURCE_ROOT and BUILD_ROOT are required")
endif()

set(output_directory "${BUILD_ROOT}/cli-smoke-output")
file(REMOVE_RECURSE "${output_directory}")

execute_process(
    COMMAND "${CLI_PATH}" run --protocol file --sample-id 0001 --file-max-count 2 --output-dir "${output_directory}" --catalog "${SOURCE_ROOT}/Sample Logs/sample_logs.json" --quiet
    RESULT_VARIABLE cli_result
    OUTPUT_VARIABLE cli_output
    ERROR_VARIABLE cli_error
    TIMEOUT 15
)

if(NOT cli_result EQUAL 0)
    file(REMOVE_RECURSE "${output_directory}")
    message(FATAL_ERROR "CLI smoke failed (${cli_result}): ${cli_error}${cli_output}")
endif()

file(GLOB generated_logs "${output_directory}/*.log")
list(LENGTH generated_logs generated_count)
file(REMOVE_RECURSE "${output_directory}")
if(NOT generated_count EQUAL 2)
    message(FATAL_ERROR "CLI smoke generated ${generated_count} files instead of 2")
endif()
