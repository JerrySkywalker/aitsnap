# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

project(AITSnap VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The current AITSnap build baseline requires Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
include(CTest)

add_library(aitsnap-domain STATIC src/core/domain.cpp)
set_target_properties(aitsnap-domain PROPERTIES AUTOMOC OFF)
target_include_directories(aitsnap-domain PUBLIC src)
add_library(aitsnap-capture STATIC src/core/capture_service.cpp)
set_target_properties(aitsnap-capture PROPERTIES AUTOMOC OFF)
target_link_libraries(aitsnap-capture PUBLIC aitsnap-domain)
add_library(aitsnap-report STATIC src/core/report.cpp)
set_target_properties(aitsnap-report PROPERTIES AUTOMOC OFF)
target_link_libraries(aitsnap-report PUBLIC aitsnap-domain)
# Scene/context value models live in domain until their services justify targets.
if(BUILD_TESTING)
    add_executable(aitsnap-core-tests tests/core_tests.cpp)
    set_target_properties(aitsnap-core-tests PROPERTIES AUTOMOC OFF)
    target_link_libraries(aitsnap-core-tests PRIVATE aitsnap-capture aitsnap-report)
    add_test(NAME core COMMAND aitsnap-core-tests)
endif()

set(AITSNAP_QT_MAJOR "6" CACHE STRING "Explicit Qt major version: 5 or 6")
set_property(CACHE AITSNAP_QT_MAJOR PROPERTY STRINGS 5 6)
option(AITSNAP_BUILD_COMPAT_PROBE "Build the non-product Windows compatibility probe" OFF)
option(AITSNAP_BUILD_RESEARCH_PROBE "Build synthetic capture/scene feasibility probe" OFF)

if(AITSNAP_QT_MAJOR STREQUAL "6")
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The Qt 6 Windows 10/11 build requires x64.")
    endif()
    find_package(Qt6 6.12...<6.13 REQUIRED COMPONENTS Widgets)
elseif(AITSNAP_QT_MAJOR STREQUAL "5")
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
else()
    message(FATAL_ERROR "AITSNAP_QT_MAJOR must be 5 or 6; no automatic fallback is used.")
endif()

add_library(aitsnap-adapters STATIC
    src/adapters/win_capture.cpp
    src/adapters/json_store.cpp
    src/adapters/hotkeys.cpp
    src/adapters/capture_files.cpp
)
target_link_libraries(aitsnap-adapters PUBLIC aitsnap-capture Qt${AITSNAP_QT_MAJOR}::Widgets PRIVATE gdi32 user32)
target_include_directories(aitsnap-adapters PUBLIC src)
add_executable(aitsnap WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
    src/compat/qt_widgets_compat.h
)

target_link_libraries(aitsnap PRIVATE aitsnap-adapters)
set(aitsnap_targets aitsnap aitsnap-adapters)
if(BUILD_TESTING)
    add_executable(aitsnap-desktop-tests tests/desktop_tests.cpp src/main_window.cpp)
    target_link_libraries(aitsnap-desktop-tests PRIVATE aitsnap-adapters dwmapi)
    add_test(NAME desktop-adapters COMMAND aitsnap-desktop-tests)
    list(APPEND aitsnap_targets aitsnap-desktop-tests)
endif()
if(AITSNAP_BUILD_RESEARCH_PROBE)
    add_executable(aitsnap-capture-scene-probe probes/capture_scene_probe.cpp)
    target_include_directories(aitsnap-capture-scene-probe PRIVATE src)
    target_link_libraries(aitsnap-capture-scene-probe PRIVATE gdi32 user32 dwmapi)
    list(APPEND aitsnap_targets aitsnap-capture-scene-probe)
endif()
if(AITSNAP_BUILD_COMPAT_PROBE)
    add_executable(aitsnap-compat-probe probes/windows_compat_probe.cpp)
    target_include_directories(aitsnap-compat-probe PRIVATE src)
    target_link_libraries(aitsnap-compat-probe PRIVATE ole32)
    list(APPEND aitsnap_targets aitsnap-compat-probe)
endif()

foreach(target IN LISTS aitsnap_targets)
    target_link_libraries(${target} PRIVATE Qt${AITSNAP_QT_MAJOR}::Widgets)
    if(AITSNAP_QT_MAJOR STREQUAL "5")
        # Header API floor only; this does not qualify compiler/CRT dependencies.
        target_compile_definitions(${target} PRIVATE WINVER=0x0601 _WIN32_WINNT=0x0601)
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8 /Zc:__cplusplus)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
