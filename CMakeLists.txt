# CMakeLists.txt
cmake_minimum_required(VERSION 3.28)

project(LogGenerator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

if(NOT WIN32)
    message(FATAL_ERROR "LogGenerator supports Windows 64-bit only")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LogGenerator requires a 64-bit toolchain")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
)
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_compile_definitions(imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(imgui PUBLIC d3d11 dxgi)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(OPENXLSX_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(OPENXLSX_CREATE_DOCS OFF CACHE BOOL "" FORCE)
set(OPENXLSX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(OPENXLSX_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    openxlsx
    GIT_REPOSITORY https://github.com/troldal/OpenXLSX.git
    GIT_TAG v0.5.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(openxlsx)

set(LOGGEN_CORE_SOURCES
    src/application/log_renderer.cpp
    src/application/stress_test_service.cpp
    src/infrastructure/async_file_logger.cpp
    src/infrastructure/excel_log_catalog.cpp
    src/infrastructure/schannel_transport.cpp
    src/infrastructure/tcp_transport.cpp
    src/infrastructure/transport_factory.cpp
    src/infrastructure/udp_transport.cpp
    src/infrastructure/winsock_support.cpp
)

add_library(loggen_core STATIC ${LOGGEN_CORE_SOURCES})
target_include_directories(loggen_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(loggen_core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE SECURITY_WIN32 SCHANNEL_USE_BLACKLISTS _WIN32_WINNT=0x0A00)
target_link_libraries(loggen_core PUBLIC OpenXLSX::OpenXLSX PRIVATE ws2_32 secur32 crypt32)

add_executable(LogGenerator WIN32
    src/main.cpp
    src/presentation/app.cpp
    src/presentation/d3d11_context.cpp
    src/presentation/ui_theme.cpp
)
target_include_directories(LogGenerator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(LogGenerator PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)
target_link_libraries(LogGenerator PRIVATE loggen_core imgui d3d11 dxgi dwmapi)
add_custom_command(TARGET LogGenerator POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Sample Logs" "$<TARGET_FILE_DIR:LogGenerator>/Sample Logs")

enable_testing()
add_executable(LogGeneratorTests
    tests/test_main.cpp
    tests/async_file_logger_tests.cpp
    tests/log_renderer_tests.cpp
    tests/excel_log_catalog_tests.cpp
)
target_include_directories(LogGeneratorTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_compile_definitions(LogGeneratorTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00 LOGGEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(LogGeneratorTests PRIVATE loggen_core)
add_test(NAME LogGeneratorTests COMMAND LogGeneratorTests)

function(loggen_configure_target target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /WX /utf-8 /permissive- /Zc:__cplusplus /EHsc /MP /external:anglebrackets /external:W0 "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Release>:/GL>" "$<$<CONFIG:Release>:/arch:AVX2>")
        target_link_options(${target_name} PRIVATE "$<$<CONFIG:Release>:/LTCG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
    endif()
endfunction()

loggen_configure_target(loggen_core)
loggen_configure_target(LogGenerator)
loggen_configure_target(LogGeneratorTests)

install(TARGETS LogGenerator RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Sample Logs" DESTINATION .)
