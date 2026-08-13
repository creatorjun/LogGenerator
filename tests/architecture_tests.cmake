# tests/architecture_tests.cmake
if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

function(assert_no_fragment source_file fragment boundary)
    file(READ "${source_file}" source_content)
    string(FIND "${source_content}" "${fragment}" match_index)
    if(NOT match_index EQUAL -1)
        file(RELATIVE_PATH relative_file "${SOURCE_ROOT}" "${source_file}")
        message(FATAL_ERROR "${boundary}: ${relative_file} contains forbidden dependency '${fragment}'")
    endif()
endfunction()

file(GLOB_RECURSE domain_files "${SOURCE_ROOT}/src/domain/*.cpp" "${SOURCE_ROOT}/src/domain/*.hpp")
file(GLOB_RECURSE application_files "${SOURCE_ROOT}/src/application/*.cpp" "${SOURCE_ROOT}/src/application/*.hpp")
file(GLOB_RECURSE application_port_files "${SOURCE_ROOT}/src/application/ports/*.cpp" "${SOURCE_ROOT}/src/application/ports/*.hpp")
file(GLOB_RECURSE infrastructure_files "${SOURCE_ROOT}/src/infrastructure/*.cpp" "${SOURCE_ROOT}/src/infrastructure/*.hpp")
file(GLOB_RECURSE presentation_files "${SOURCE_ROOT}/src/presentation/*.cpp" "${SOURCE_ROOT}/src/presentation/*.hpp")

foreach(source_file IN LISTS domain_files)
    foreach(fragment IN ITEMS "#include \"application/" "#include \"infrastructure/" "#include \"presentation/" "#include <Windows.h>" "#include <WinSock2.h>" "#include <imgui.h>" "#include <nlohmann/json.hpp>")
        assert_no_fragment("${source_file}" "${fragment}" "Domain boundary violation")
    endforeach()
endforeach()

foreach(source_file IN LISTS application_files)
    foreach(fragment IN ITEMS "#include \"infrastructure/" "#include \"presentation/" "#include <Windows.h>" "#include <WinSock2.h>" "#include <WS2tcpip.h>" "#include <timeapi.h>" "#include <d3d11.h>" "#include <imgui.h>" "#include <nlohmann/json.hpp>" "SetThreadPriority(" "timeBeginPeriod(" "YieldProcessor(")
        assert_no_fragment("${source_file}" "${fragment}" "Application boundary violation")
    endforeach()
endforeach()

foreach(source_file IN LISTS application_port_files)
    assert_no_fragment("${source_file}" "#include \"application/" "Application port boundary violation")
endforeach()

foreach(source_file IN LISTS infrastructure_files)
    assert_no_fragment("${source_file}" "#include \"presentation/" "Infrastructure boundary violation")
endforeach()

foreach(source_file IN LISTS presentation_files)
    assert_no_fragment("${source_file}" "#include \"infrastructure/" "Presentation boundary violation")
endforeach()
