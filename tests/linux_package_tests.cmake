# tests/linux_package_tests.cmake
foreach(required_variable PACKAGE_ROOT ARCHIVE_PATH)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(required_paths
    "LogGenerator"
    "LogGeneratorCli"
    "run-loggenerator.sh"
    "install-shortcuts.sh"
    "README.md"
    "BUILD.md"
    "Sample Logs/sample_logs.json"
    "fonts/NotoSansKR-Regular.otf"
    "fonts/NotoSansKR-Bold.otf"
    "fonts/OFL.txt"
    "resources/log.ico"
)
foreach(required_path IN LISTS required_paths)
    if(NOT EXISTS "${PACKAGE_ROOT}/${required_path}")
        message(FATAL_ERROR "Linux package is missing ${required_path}")
    endif()
endforeach()

file(SHA256 "${PACKAGE_ROOT}/fonts/NotoSansKR-Regular.otf" regular_font_hash)
file(SHA256 "${PACKAGE_ROOT}/fonts/NotoSansKR-Bold.otf" bold_font_hash)
file(SHA256 "${PACKAGE_ROOT}/fonts/OFL.txt" font_license_hash)
if(NOT regular_font_hash STREQUAL "69975a0ac8472717870aefeab0a4d52739308d90856b9955313b2ad5e0148d68")
    message(FATAL_ERROR "Bundled regular Korean font hash is invalid")
endif()
if(NOT bold_font_hash STREQUAL "5a6ceb287ed2fc6cfc6213144ebea68cbd94b20fc9eb873d8486493bf02d9bda")
    message(FATAL_ERROR "Bundled bold Korean font hash is invalid")
endif()
if(NOT font_license_hash STREQUAL "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2")
    message(FATAL_ERROR "Bundled Korean font license hash is invalid")
endif()
if(NOT EXISTS "${ARCHIVE_PATH}")
    message(FATAL_ERROR "Linux ZIP archive was not created")
endif()

set(extraction_directory "${PACKAGE_ROOT}-archive-test")
file(REMOVE_RECURSE "${extraction_directory}")
file(MAKE_DIRECTORY "${extraction_directory}")
file(ARCHIVE_EXTRACT INPUT "${ARCHIVE_PATH}" DESTINATION "${extraction_directory}")
foreach(required_path IN LISTS required_paths)
    if(NOT EXISTS "${extraction_directory}/LogGenerator/${required_path}")
        message(FATAL_ERROR "Linux ZIP archive is missing ${required_path}")
    endif()
endforeach()
