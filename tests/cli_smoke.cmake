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
    ENCODING UTF-8
    TIMEOUT 15
)

if(NOT cli_result EQUAL 0)
    file(REMOVE_RECURSE "${output_directory}")
    message(FATAL_ERROR "CLI smoke failed (${cli_result}): ${cli_error}${cli_output}")
endif()

string(FIND "${cli_output}" "FILE 생성 파일 개수 제한에 도달하여 자동 중지했습니다." cli_utf8_message_index)
if(cli_utf8_message_index EQUAL -1)
    file(REMOVE_RECURSE "${output_directory}")
    message(FATAL_ERROR "CLI smoke output does not contain the expected UTF-8 status message: ${cli_output}")
endif()

file(GLOB generated_logs "${output_directory}/*.log")
list(LENGTH generated_logs generated_count)
file(REMOVE_RECURSE "${output_directory}")
if(NOT generated_count EQUAL 2)
    message(FATAL_ERROR "CLI smoke generated ${generated_count} files instead of 2")
endif()

set(all_catalog "${BUILD_ROOT}/cli-smoke-all-catalog.json")
file(WRITE "${all_catalog}" "{\n  \"schema_version\": 1,\n  \"logs\": [\n    {\"id\": \"0001\", \"name\": \"One\", \"sample\": \"ROUND_ROBIN_1\"},\n    {\"id\": \"0002\", \"name\": \"Two\", \"sample\": \"ROUND_ROBIN_2\"},\n    {\"id\": \"0003\", \"name\": \"Three\", \"sample\": \"ROUND_ROBIN_3\"}\n  ]\n}\n")

execute_process(
    COMMAND "${CLI_PATH}" run --all --protocol file --file-max-count 6 --output-dir "${output_directory}" --catalog "${all_catalog}" --quiet
    RESULT_VARIABLE all_result
    OUTPUT_VARIABLE all_output
    ERROR_VARIABLE all_error
    ENCODING UTF-8
    TIMEOUT 15
)

if(NOT all_result EQUAL 0)
    file(REMOVE_RECURSE "${output_directory}")
    file(REMOVE "${all_catalog}")
    message(FATAL_ERROR "CLI --all smoke failed (${all_result}): ${all_error}${all_output}")
endif()

file(GLOB all_logs "${output_directory}/*.log")
list(SORT all_logs)
list(LENGTH all_logs all_count)
if(NOT all_count EQUAL 6)
    file(REMOVE_RECURSE "${output_directory}")
    file(REMOVE "${all_catalog}")
    message(FATAL_ERROR "CLI --all smoke generated ${all_count} files instead of 6")
endif()

set(expected_round_robin ROUND_ROBIN_1 ROUND_ROBIN_2 ROUND_ROBIN_3 ROUND_ROBIN_1 ROUND_ROBIN_2 ROUND_ROBIN_3)
foreach(index RANGE 0 5)
    list(GET all_logs ${index} generated_file)
    list(GET expected_round_robin ${index} expected_payload)
    file(READ "${generated_file}" actual_payload)
    string(STRIP "${actual_payload}" actual_payload)
    if(NOT actual_payload STREQUAL expected_payload)
        file(REMOVE_RECURSE "${output_directory}")
        file(REMOVE "${all_catalog}")
        message(FATAL_ERROR "CLI --all round-robin mismatch at ${index}: ${actual_payload} != ${expected_payload}")
    endif()
endforeach()

file(REMOVE_RECURSE "${output_directory}")
file(REMOVE "${all_catalog}")
