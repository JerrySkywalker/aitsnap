# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.24)

project(AITSnap VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The current AITSnap build baseline requires Windows x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# Qt 6.12 is the Windows 10/11 baseline. Review platform support before
# adopting a newer minor version (Qt 6.12 is the last to support Windows 10).
find_package(Qt6 6.12...<6.13 REQUIRED COMPONENTS Widgets)

add_executable(aitsnap WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(aitsnap PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(aitsnap PRIVATE /W4 /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aitsnap PRIVATE -Wall -Wextra -Wpedantic)
endif()
