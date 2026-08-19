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
    foreach(fragment IN ITEMS "#include \"application/" "#include \"infrastructure/" "#include \"presentation/" "#include <Windows.h>" "#include <WinSock2.h>" "#include <sys/" "#include <openssl/" "#include <GLFW/" "#include <imgui.h>" "#include <nlohmann/json.hpp>")
        assert_no_fragment("${source_file}" "${fragment}" "Domain boundary violation")
    endforeach()
endforeach()

foreach(source_file IN LISTS application_files)
    foreach(fragment IN ITEMS "#include \"infrastructure/" "#include \"presentation/" "#include <Windows.h>" "#include <WinSock2.h>" "#include <WS2tcpip.h>" "#include <timeapi.h>" "#include <d3d11.h>" "#include <sys/" "#include <netinet/" "#include <unistd.h>" "#include <openssl/" "#include <GLFW/" "#include <GL/" "#include <imgui.h>" "#include <nlohmann/json.hpp>" "SetThreadPriority(" "timeBeginPeriod(" "YieldProcessor(" "gmtime_s(" "localtime_s(" "glfw" "SSL_")
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
    foreach(fragment IN ITEMS
            "#include \"infrastructure/"
            "#include \"application/log_catalog_service.hpp\""
            "#include \"application/stress_test_service.hpp\""
            "#include \"application/log_renderer.hpp\""
            "#include \"application/privacy_anonymizer.hpp\"")
        assert_no_fragment("${source_file}" "${fragment}" "Presentation boundary violation")
    endforeach()
endforeach()

set(transport_factory_header "${SOURCE_ROOT}/src/infrastructure/transport_factory.hpp")
foreach(fragment IN ITEMS "infrastructure/socket_support.hpp" "WinSock2.h" "sys/socket.h" "SocketRuntime")
    assert_no_fragment("${transport_factory_header}" "${fragment}" "Composition-facing infrastructure header leak")
endforeach()

if(EXISTS "${SOURCE_ROOT}/src/domain/log_level.hpp")
    message(FATAL_ERROR "Domain technical concern violation: logger severity belongs to the logger output port")
endif()

file(READ "${SOURCE_ROOT}/CMakeLists.txt" build_definition)
string(FIND "${build_definition}" "add_library(loggen_gui_presentation STATIC" gui_adapter_index)
if(gui_adapter_index EQUAL -1)
    message(FATAL_ERROR "Build boundary violation: GUI presentation must be an independent adapter target")
endif()
foreach(adapter_target IN ITEMS loggen_gui_presentation loggen_cli_presentation)
    string(REGEX MATCH "target_link_libraries\\(${adapter_target}[^\\)]*loggen_infrastructure" reverse_link "${build_definition}")
    if(reverse_link)
        message(FATAL_ERROR "Build boundary violation: ${adapter_target} links directly to infrastructure")
    endif()
endforeach()
