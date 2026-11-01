# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

project(AITSnap VERSION 0.7.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The current AITSnap build baseline requires Windows.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
include(CTest)

add_library(aitsnap-domain STATIC src/core/domain.cpp src/core/region_edit.cpp)
set_target_properties(aitsnap-domain PROPERTIES AUTOMOC OFF)
target_include_directories(aitsnap-domain PUBLIC src)
add_library(aitsnap-capture STATIC src/core/capture_service.cpp)
set_target_properties(aitsnap-capture PROPERTIES AUTOMOC OFF)
target_link_libraries(aitsnap-capture PUBLIC aitsnap-domain)
add_library(aitsnap-report STATIC src/core/report.cpp)
set_target_properties(aitsnap-report PROPERTIES AUTOMOC OFF)
target_link_libraries(aitsnap-report PUBLIC aitsnap-domain)
add_library(aitsnap-context STATIC src/core/context.cpp)
set_target_properties(aitsnap-context PROPERTIES AUTOMOC OFF)
target_link_libraries(aitsnap-context PUBLIC aitsnap-domain)
add_library(aitsnap-scene STATIC src/core/scene_geometry.cpp)
set_target_properties(aitsnap-scene PROPERTIES AUTOMOC OFF)
target_link_libraries(aitsnap-scene PUBLIC aitsnap-domain)
# Scene value models live in domain until their services justify targets.
if(BUILD_TESTING)
    add_executable(aitsnap-core-tests tests/core_tests.cpp)
    set_target_properties(aitsnap-core-tests PROPERTIES AUTOMOC OFF)
    target_link_libraries(aitsnap-core-tests PRIVATE aitsnap-capture aitsnap-report)
    add_test(NAME core COMMAND aitsnap-core-tests)
    add_executable(aitsnap-context-tests tests/context_tests.cpp)
    set_target_properties(aitsnap-context-tests PROPERTIES AUTOMOC OFF)
    target_link_libraries(aitsnap-context-tests PRIVATE aitsnap-context)
    add_test(NAME context COMMAND aitsnap-context-tests)
endif()

set(AITSNAP_QT_MAJOR "6" CACHE STRING "Explicit Qt major version: 5 or 6")
set_property(CACHE AITSNAP_QT_MAJOR PROPERTY STRINGS 5 6)
option(AITSNAP_BUILD_COMPAT_PROBE "Build the non-product Windows compatibility probe" OFF)
option(AITSNAP_BUILD_RESEARCH_PROBE "Build synthetic capture/scene feasibility probe" OFF)
option(AITSNAP_BUILD_ASSET_TOOLS "Build SVG-to-PNG authoring tool (not shipped)" OFF)

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

if(AITSNAP_BUILD_ASSET_TOOLS)
    find_package(Qt${AITSNAP_QT_MAJOR} REQUIRED COMPONENTS Svg)
    add_executable(aitsnap-render-assets tools/render_assets.cpp)
    target_link_libraries(aitsnap-render-assets PRIVATE Qt${AITSNAP_QT_MAJOR}::Widgets Qt${AITSNAP_QT_MAJOR}::Svg)
endif()

add_library(aitsnap-adapters STATIC
    src/adapters/win_capture.cpp
    src/adapters/json_store.cpp
    src/adapters/hotkeys.cpp
    src/adapters/capture_files.cpp
    src/adapters/report_queue.cpp
    src/adapters/context_store.cpp
    src/adapters/host_journal.cpp
    src/adapters/local_pipe.cpp
    src/desktop/region_editor.cpp
    src/adapters/scene_render.cpp
    src/desktop/scene_editor.cpp
    src/desktop/appearance.cpp
    src/desktop/workspace_views.cpp
    resources/aitsnap.qrc
)
target_link_libraries(aitsnap-adapters PUBLIC aitsnap-capture aitsnap-scene aitsnap-context Qt${AITSNAP_QT_MAJOR}::Widgets PRIVATE gdi32 user32 dwmapi wtsapi32 advapi32)
target_include_directories(aitsnap-adapters PUBLIC src)
add_executable(aitsnap-word-bridge src/adapters/word_bridge_main.cpp)
target_link_libraries(aitsnap-word-bridge PRIVATE aitsnap-adapters ole32 oleaut32 psapi)
add_executable(aitsnap-cli src/cli_main.cpp)
target_link_libraries(aitsnap-cli PRIVATE aitsnap-adapters)
set(aitsnap_ui_sources src/main_window.cpp src/desktop/host_controller.cpp src/desktop/workspace_ui.cpp src/desktop/workspace_browser.cpp)
add_executable(aitsnap WIN32
    src/main.cpp
    ${aitsnap_ui_sources}
    src/main_window.h
    src/compat/qt_widgets_compat.h
)

target_link_libraries(aitsnap PRIVATE aitsnap-adapters psapi)
set(aitsnap_targets aitsnap aitsnap-adapters aitsnap-word-bridge aitsnap-cli)
if(BUILD_TESTING)
    add_executable(aitsnap-desktop-tests tests/desktop_tests.cpp ${aitsnap_ui_sources})
    target_link_libraries(aitsnap-desktop-tests PRIVATE aitsnap-adapters dwmapi)
    add_test(NAME desktop-adapters COMMAND aitsnap-desktop-tests)
    list(APPEND aitsnap_targets aitsnap-desktop-tests)
    add_executable(aitsnap-appearance-tests tests/appearance_tests.cpp ${aitsnap_ui_sources})
    target_link_libraries(aitsnap-appearance-tests PRIVATE aitsnap-adapters)
    add_test(NAME appearance COMMAND aitsnap-appearance-tests)
    list(APPEND aitsnap_targets aitsnap-appearance-tests)
    add_executable(aitsnap-scene-tests tests/scene_tests.cpp)
    target_link_libraries(aitsnap-scene-tests PRIVATE aitsnap-adapters)
    add_test(NAME scene COMMAND aitsnap-scene-tests)
    list(APPEND aitsnap_targets aitsnap-scene-tests)
    add_executable(aitsnap-report-tests tests/report_tests.cpp)
    target_link_libraries(aitsnap-report-tests PRIVATE aitsnap-adapters)
    add_test(NAME report-queue COMMAND aitsnap-report-tests)
    list(APPEND aitsnap_targets aitsnap-report-tests)
    # Office-dependent synthetic integration is opt-in, never an ordinary CTest.
    add_executable(aitsnap-word-bridge-tests tests/word_bridge_tests.cpp)
    target_link_libraries(aitsnap-word-bridge-tests PRIVATE aitsnap-adapters ole32 oleaut32)
    list(APPEND aitsnap_targets aitsnap-word-bridge-tests)
    add_executable(aitsnap-word-document-tests tests/word_document_tests.cpp)
    target_link_libraries(aitsnap-word-document-tests PRIVATE aitsnap-adapters ole32 oleaut32)
    add_dependencies(aitsnap-word-document-tests aitsnap-word-bridge)
    list(APPEND aitsnap_targets aitsnap-word-document-tests)
    add_executable(aitsnap-context-store-tests tests/context_store_tests.cpp)
    target_link_libraries(aitsnap-context-store-tests PRIVATE aitsnap-adapters)
    add_test(NAME context-store COMMAND aitsnap-context-store-tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/synthetic-ait")
    list(APPEND aitsnap_targets aitsnap-context-store-tests)
    add_executable(aitsnap-host-tests tests/host_tests.cpp)
    target_link_libraries(aitsnap-host-tests PRIVATE aitsnap-adapters)
    add_test(NAME local-host COMMAND aitsnap-host-tests)
    list(APPEND aitsnap_targets aitsnap-host-tests)
    add_executable(aitsnap-ipc-capture-tests tests/ipc_capture_tests.cpp ${aitsnap_ui_sources})
    target_link_libraries(aitsnap-ipc-capture-tests PRIVATE aitsnap-adapters dwmapi)
    add_dependencies(aitsnap-ipc-capture-tests aitsnap-cli aitsnap-word-bridge)
    list(APPEND aitsnap_targets aitsnap-ipc-capture-tests)
    add_executable(aitsnap-benchmark EXCLUDE_FROM_ALL probes/performance_probe.cpp)
    target_link_libraries(aitsnap-benchmark PRIVATE aitsnap-adapters psapi dwmapi)
    add_dependencies(aitsnap-benchmark aitsnap-word-bridge)
    list(APPEND aitsnap_targets aitsnap-benchmark)
endif()
if(AITSNAP_BUILD_RESEARCH_PROBE)
    add_executable(aitsnap-word-probe probes/word_native_probe.cpp)
    target_include_directories(aitsnap-word-probe PRIVATE src)
    target_link_libraries(aitsnap-word-probe PRIVATE ole32 oleaut32)
    list(APPEND aitsnap_targets aitsnap-word-probe)
    add_executable(aitsnap-capture-scene-probe probes/capture_scene_probe.cpp)
    target_include_directories(aitsnap-capture-scene-probe PRIVATE src)
    target_link_libraries(aitsnap-capture-scene-probe PRIVATE aitsnap-adapters gdi32 user32 dwmapi wtsapi32)
    list(APPEND aitsnap_targets aitsnap-capture-scene-probe)
endif()
if(AITSNAP_BUILD_COMPAT_PROBE)
    add_executable(aitsnap-compat-probe probes/windows_compat_probe.cpp)
    target_include_directories(aitsnap-compat-probe PRIVATE src)
    target_link_libraries(aitsnap-compat-probe PRIVATE ole32)
    list(APPEND aitsnap_targets aitsnap-compat-probe)
endif()

foreach(target IN LISTS aitsnap_targets)
    target_compile_definitions(${target} PRIVATE AITSNAP_VERSION="${PROJECT_VERSION}")
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
