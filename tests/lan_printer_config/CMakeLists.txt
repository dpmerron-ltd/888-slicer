# Lightweight runner for the same parser tests used by the libslic3r suite.
# This does not require the slicer's graphics/geometry dependencies.
cmake_minimum_required(VERSION 3.16)
project(lan_printer_config_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
add_subdirectory(../catch2 catch2)
add_executable(lan_printer_config_tests
    ../libslic3r/test_lan_printer_config.cpp
    ../../src/libslic3r/LanPrinterConfig.cpp
    i18n_stub.cpp)
target_include_directories(lan_printer_config_tests PRIVATE ../../src ../../deps_src)
target_link_libraries(lan_printer_config_tests PRIVATE Catch2::Catch2WithMain)
add_test(NAME lan_printer_config COMMAND lan_printer_config_tests)
