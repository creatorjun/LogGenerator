# CMakeLists.txt
cmake_minimum_required(VERSION 3.26)

project(LogGenerator VERSION 1.0.0 LANGUAGES CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "LogGenerator requires a 64-bit toolchain")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
option(LOGGEN_BUILD_GUI "Build the desktop GUI" ON)
option(LOGGEN_BUILD_BENCHMARKS "Build performance benchmark executables" OFF)
option(LOGGEN_INSTALL_LINUX_SHORTCUTS "Install the Linux application menu and desktop shortcuts after a GUI build" ON)

include(FetchContent)
find_package(Threads REQUIRED)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LOGGEN_BUILD_GUI)
    set(LOGGEN_LINUX_FONT_DIRECTORY "${CMAKE_BINARY_DIR}/linux-fonts")
    file(MAKE_DIRECTORY "${LOGGEN_LINUX_FONT_DIRECTORY}")

    function(loggen_download_verified_file url destination expected_sha256)
        set(download_required TRUE)
        if(EXISTS "${destination}")
            file(SHA256 "${destination}" existing_sha256)
            if(existing_sha256 STREQUAL expected_sha256)
                set(download_required FALSE)
            endif()
        endif()
        if(download_required)
            set(download_path "${destination}.download")
            file(REMOVE "${download_path}")
            file(DOWNLOAD "${url}" "${download_path}" EXPECTED_HASH "SHA256=${expected_sha256}" TLS_VERIFY ON INACTIVITY_TIMEOUT 60 TIMEOUT 300 STATUS download_status)
            list(GET download_status 0 download_result)
            list(GET download_status 1 download_error)
            if(NOT download_result EQUAL 0)
                file(REMOVE "${download_path}")
                message(FATAL_ERROR "Download failed for ${url}: ${download_error}")
            endif()
            file(RENAME "${download_path}" "${destination}")
        endif()
    endfunction()

    set(LOGGEN_NOTO_CJK_COMMIT "f8d157532fbfaeda587e826d4cd5b21a49186f7c")
    set(LOGGEN_NOTO_CJK_BASE_URL "https://raw.githubusercontent.com/notofonts/noto-cjk/${LOGGEN_NOTO_CJK_COMMIT}/Sans")
    loggen_download_verified_file("${LOGGEN_NOTO_CJK_BASE_URL}/SubsetOTF/KR/NotoSansKR-Regular.otf" "${LOGGEN_LINUX_FONT_DIRECTORY}/NotoSansKR-Regular.otf" "69975a0ac8472717870aefeab0a4d52739308d90856b9955313b2ad5e0148d68")
    loggen_download_verified_file("${LOGGEN_NOTO_CJK_BASE_URL}/SubsetOTF/KR/NotoSansKR-Bold.otf" "${LOGGEN_LINUX_FONT_DIRECTORY}/NotoSansKR-Bold.otf" "5a6ceb287ed2fc6cfc6213144ebea68cbd94b20fc9eb873d8486493bf02d9bda")
    loggen_download_verified_file("${LOGGEN_NOTO_CJK_BASE_URL}/LICENSE" "${LOGGEN_LINUX_FONT_DIRECTORY}/OFL.txt" "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2")
endif()

if(LOGGEN_BUILD_GUI)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

set(LOGGEN_IMGUI_SOURCES
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
)

if(WIN32)
    list(APPEND LOGGEN_IMGUI_SOURCES
        "${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    )
else()
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    if(APPLE)
        set(GLFW_BUILD_COCOA ON CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    else()
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(glfw)
    find_package(OpenGL REQUIRED)
    list(APPEND LOGGEN_IMGUI_SOURCES
        "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
    )
endif()

add_library(imgui STATIC ${LOGGEN_IMGUI_SOURCES})
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
if(WIN32)
    target_compile_definitions(imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(imgui PUBLIC d3d11 dxgi)
else()
    target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
endif()
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

set(LOGGEN_APPLICATION_SOURCES
    src/application/log_catalog_service.cpp
    src/application/log_preparation_cache.cpp
    src/application/log_renderer.cpp
    src/application/privacy_anonymizer.cpp
    src/application/stress_test_service.cpp
)

set(LOGGEN_INFRASTRUCTURE_SOURCES
    src/infrastructure/async_file_logger.cpp
    src/infrastructure/file_transport.cpp
    src/infrastructure/json_log_catalog.cpp
    src/infrastructure/socket_support.cpp
    src/infrastructure/tcp_transport.cpp
    src/infrastructure/transport_factory.cpp
    src/infrastructure/udp_transport.cpp
)

if(WIN32)
    list(APPEND LOGGEN_INFRASTRUCTURE_SOURCES
        src/infrastructure/schannel_transport.cpp
        src/infrastructure/windows_execution_runtime.cpp
    )
else()
    find_package(OpenSSL QUIET)
    if(APPLE AND NOT OpenSSL_FOUND)
        find_program(LOGGEN_HOMEBREW_EXECUTABLE brew)
        if(LOGGEN_HOMEBREW_EXECUTABLE)
            execute_process(
                COMMAND "${LOGGEN_HOMEBREW_EXECUTABLE}" --prefix openssl@3
                OUTPUT_VARIABLE LOGGEN_HOMEBREW_OPENSSL_ROOT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                RESULT_VARIABLE LOGGEN_HOMEBREW_OPENSSL_RESULT
            )
            if(LOGGEN_HOMEBREW_OPENSSL_RESULT EQUAL 0 AND EXISTS "${LOGGEN_HOMEBREW_OPENSSL_ROOT}")
                set(OPENSSL_ROOT_DIR "${LOGGEN_HOMEBREW_OPENSSL_ROOT}")
            endif()
        endif()
    endif()
    find_package(OpenSSL REQUIRED)
    list(APPEND LOGGEN_INFRASTRUCTURE_SOURCES
        src/infrastructure/openssl_transport.cpp
        src/infrastructure/posix_execution_runtime.cpp
    )
endif()

add_library(loggen_application STATIC ${LOGGEN_APPLICATION_SOURCES})
target_include_directories(loggen_application PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(loggen_infrastructure STATIC ${LOGGEN_INFRASTRUCTURE_SOURCES})
target_include_directories(loggen_infrastructure PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(loggen_infrastructure PUBLIC loggen_application PRIVATE nlohmann_json::nlohmann_json)
if(WIN32)
    target_compile_definitions(loggen_infrastructure PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE SECURITY_WIN32 SCHANNEL_USE_BLACKLISTS _WIN32_WINNT=0x0A00)
    target_link_libraries(loggen_infrastructure PRIVATE ws2_32 secur32 crypt32 winmm)
else()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions(loggen_infrastructure PRIVATE _GNU_SOURCE)
    endif()
    target_link_libraries(loggen_infrastructure PRIVATE OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
endif()

add_library(loggen_cli_presentation STATIC
    src/presentation/cli_app.cpp
)
target_include_directories(loggen_cli_presentation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(loggen_cli_presentation PUBLIC loggen_application)

add_executable(LogGeneratorCli
    src/cli_main.cpp
)
if(NOT LOGGEN_BUILD_GUI)
    set_target_properties(LogGeneratorCli PROPERTIES OUTPUT_NAME LogGenerator)
endif()
target_include_directories(LogGeneratorCli PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(LogGeneratorCli PRIVATE loggen_cli_presentation loggen_infrastructure Threads::Threads)
if(WIN32)
    target_compile_definitions(LogGeneratorCli PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)
endif()
add_custom_target(loggen_copy_sample_logs
    COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:LogGeneratorCli>/Sample Logs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Sample Logs" "$<TARGET_FILE_DIR:LogGeneratorCli>/Sample Logs"
    VERBATIM
)
add_dependencies(LogGeneratorCli loggen_copy_sample_logs)

if(LOGGEN_BUILD_GUI)
set(LOGGEN_GUI_PRESENTATION_SOURCES
    src/presentation/app.cpp
    src/presentation/ui_theme.cpp
)

if(WIN32)
    list(APPEND LOGGEN_GUI_PRESENTATION_SOURCES
        src/presentation/d3d11_context.cpp
        src/presentation/windows_icon.cpp
    )
endif()

add_library(loggen_gui_presentation STATIC ${LOGGEN_GUI_PRESENTATION_SOURCES})
target_include_directories(loggen_gui_presentation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(loggen_gui_presentation PUBLIC loggen_application PRIVATE imgui)
if(WIN32)
    target_compile_definitions(loggen_gui_presentation PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_link_libraries(loggen_gui_presentation PRIVATE d3d11 dxgi dwmapi shell32)
elseif(APPLE)
    target_compile_definitions(loggen_gui_presentation PRIVATE GL_SILENCE_DEPRECATION)
endif()

if(WIN32)
    add_executable(LogGenerator WIN32 src/main.cpp resources/windows/log_generator.rc)
else()
    add_executable(LogGenerator src/main.cpp)
endif()
target_include_directories(LogGenerator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(LogGenerator PRIVATE loggen_application loggen_infrastructure loggen_gui_presentation)
if(WIN32)
    target_compile_definitions(LogGenerator PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)
else()
    target_link_libraries(LogGenerator PRIVATE Threads::Threads)
endif()
add_dependencies(LogGenerator LogGeneratorCli)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_target(loggen_copy_linux_fonts
        COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:LogGenerator>/fonts"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${LOGGEN_LINUX_FONT_DIRECTORY}" "$<TARGET_FILE_DIR:LogGenerator>/fonts"
        VERBATIM
    )
    add_dependencies(LogGenerator loggen_copy_linux_fonts)

    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" LOGGEN_LINUX_PACKAGE_ARCHITECTURE)
    if(LOGGEN_LINUX_PACKAGE_ARCHITECTURE MATCHES "^(amd64|x86_64)$")
        set(LOGGEN_LINUX_PACKAGE_ARCHITECTURE "x86_64")
    elseif(LOGGEN_LINUX_PACKAGE_ARCHITECTURE MATCHES "^(aarch64|arm64)$")
        set(LOGGEN_LINUX_PACKAGE_ARCHITECTURE "aarch64")
    endif()
    set(LOGGEN_LINUX_PACKAGE_ROOT "${CMAKE_BINARY_DIR}/package/LogGenerator")
    set(LOGGEN_LINUX_PACKAGE_ARCHIVE "${CMAKE_BINARY_DIR}/dist/LogGenerator-${PROJECT_VERSION}-oracle-linux-${LOGGEN_LINUX_PACKAGE_ARCHITECTURE}.zip")
    set(LOGGEN_LINUX_PACKAGE_COMMAND
        COMMAND ${CMAKE_COMMAND}
            "-DPACKAGE_ROOT=${LOGGEN_LINUX_PACKAGE_ROOT}"
            "-DARCHIVE_PATH=${LOGGEN_LINUX_PACKAGE_ARCHIVE}"
            "-DGUI_EXECUTABLE=$<TARGET_FILE:LogGenerator>"
            "-DCLI_EXECUTABLE=$<TARGET_FILE:LogGeneratorCli>"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DFONT_DIRECTORY=${LOGGEN_LINUX_FONT_DIRECTORY}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/scripts/package-linux.cmake"
    )
    if(LOGGEN_INSTALL_LINUX_SHORTCUTS)
        find_program(LOGGEN_BASH_EXECUTABLE bash REQUIRED)
        list(APPEND LOGGEN_LINUX_PACKAGE_COMMAND COMMAND "${LOGGEN_BASH_EXECUTABLE}" "${LOGGEN_LINUX_PACKAGE_ROOT}/install-shortcuts.sh")
    endif()
    add_custom_target(LogGeneratorLinuxPackage ALL
        ${LOGGEN_LINUX_PACKAGE_COMMAND}
        BYPRODUCTS "${LOGGEN_LINUX_PACKAGE_ARCHIVE}"
        DEPENDS LogGenerator LogGeneratorCli loggen_copy_linux_fonts
        VERBATIM
        USES_TERMINAL
    )
endif()
endif()

enable_testing()
set(LOGGEN_TEST_SOURCES
    tests/test_main.cpp
    tests/async_file_logger_tests.cpp
    tests/cli_app_tests.cpp
    tests/file_transport_tests.cpp
    tests/log_catalog_service_tests.cpp
    tests/log_renderer_tests.cpp
    tests/log_preparation_cache_tests.cpp
    tests/json_log_catalog_tests.cpp
    tests/stress_test_service_tests.cpp
    tests/udp_transport_tests.cpp
    tests/responsive_layout_tests.cpp
)
if(WIN32 AND LOGGEN_BUILD_GUI)
    list(APPEND LOGGEN_TEST_SOURCES tests/windows_icon_tests.cpp)
endif()
add_executable(LogGeneratorTests ${LOGGEN_TEST_SOURCES})
target_include_directories(LogGeneratorTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_compile_definitions(LogGeneratorTests PRIVATE LOGGEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(WIN32 AND LOGGEN_BUILD_GUI)
    target_compile_definitions(LogGeneratorTests PRIVATE LOGGEN_HAS_WINDOWS_ICON_TESTS=1)
endif()
target_link_libraries(LogGeneratorTests PRIVATE loggen_application loggen_cli_presentation loggen_infrastructure nlohmann_json::nlohmann_json)
if(WIN32)
    target_compile_definitions(LogGeneratorTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE _WIN32_WINNT=0x0A00)
    target_link_libraries(LogGeneratorTests PRIVATE user32)
else()
    target_link_libraries(LogGeneratorTests PRIVATE Threads::Threads)
endif()
add_dependencies(LogGeneratorTests LogGeneratorCli)
if(WIN32 AND LOGGEN_BUILD_GUI)
    add_dependencies(LogGeneratorTests LogGenerator)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LOGGEN_BUILD_GUI)
    add_dependencies(LogGeneratorTests LogGeneratorLinuxPackage)
endif()
add_test(NAME LogGeneratorTests COMMAND LogGeneratorTests)
add_test(NAME LogGeneratorArchitecture COMMAND ${CMAKE_COMMAND} "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/architecture_tests.cmake")
add_test(NAME LogGeneratorCliHelp COMMAND LogGeneratorCli --help)
add_test(NAME LogGeneratorCliSmoke COMMAND ${CMAKE_COMMAND} "-DCLI_PATH=$<TARGET_FILE:LogGeneratorCli>" "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DBUILD_ROOT=${CMAKE_BINARY_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_smoke.cmake")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LOGGEN_BUILD_GUI)
    add_test(NAME LogGeneratorLinuxPackage COMMAND ${CMAKE_COMMAND} "-DPACKAGE_ROOT=${LOGGEN_LINUX_PACKAGE_ROOT}" "-DARCHIVE_PATH=${LOGGEN_LINUX_PACKAGE_ARCHIVE}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_package_tests.cmake")
endif()

if(LOGGEN_BUILD_BENCHMARKS)
    add_executable(LogGeneratorBenchmarks benchmarks/log_generator_benchmarks.cpp)
    target_include_directories(LogGeneratorBenchmarks PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(LogGeneratorBenchmarks PRIVATE loggen_application Threads::Threads)
    if(APPLE)
        add_executable(LogGeneratorMacosUdpBenchmark benchmarks/macos_udp_end_to_end_benchmark.cpp)
        target_include_directories(LogGeneratorMacosUdpBenchmark PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(LogGeneratorMacosUdpBenchmark PRIVATE LOGGEN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        target_link_libraries(LogGeneratorMacosUdpBenchmark PRIVATE loggen_application loggen_infrastructure Threads::Threads)
    endif()
endif()

function(loggen_configure_target target_name)
    if(MSVC)
        target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target_name} PRIVATE /W4 /WX /utf-8 /permissive- /Zc:__cplusplus /EHsc /MP /external:anglebrackets /external:W0 "$<$<CONFIG:Release>:/O2>" "$<$<CONFIG:Release>:/GL>" "$<$<CONFIG:Release>:/arch:AVX2>")
        target_link_options(${target_name} PRIVATE "$<$<CONFIG:Release>:/LTCG>" "$<$<CONFIG:Release>:/OPT:REF>" "$<$<CONFIG:Release>:/OPT:ICF>")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Werror "$<$<CONFIG:Release>:-O3>")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target_name} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
        endif()
    endif()
endfunction()

loggen_configure_target(loggen_application)
loggen_configure_target(loggen_infrastructure)
loggen_configure_target(loggen_cli_presentation)
loggen_configure_target(LogGeneratorCli)
if(LOGGEN_BUILD_GUI)
    loggen_configure_target(loggen_gui_presentation)
    loggen_configure_target(LogGenerator)
endif()
loggen_configure_target(LogGeneratorTests)
if(LOGGEN_BUILD_BENCHMARKS)
    loggen_configure_target(LogGeneratorBenchmarks)
    if(APPLE)
        loggen_configure_target(LogGeneratorMacosUdpBenchmark)
    endif()
endif()

install(TARGETS LogGeneratorCli RUNTIME DESTINATION .)
if(LOGGEN_BUILD_GUI)
    install(TARGETS LogGenerator RUNTIME DESTINATION .)
endif()
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Sample Logs" DESTINATION .)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LOGGEN_BUILD_GUI)
    install(DIRECTORY "${LOGGEN_LINUX_FONT_DIRECTORY}/" DESTINATION fonts)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/log.ico" DESTINATION resources)
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/run-loggenerator.sh" "${CMAKE_CURRENT_SOURCE_DIR}/packaging/linux/install-shortcuts.sh" DESTINATION .)
endif()
