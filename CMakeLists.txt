# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.16)

project(AITSnap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# Qt 5.15 is the initial dependency baseline; platform compatibility is unverified.
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)

add_executable(aitsnap WIN32
    src/main.cpp
    src/main_window.cpp
    src/main_window.h
)

target_link_libraries(aitsnap PRIVATE Qt5::Widgets)

if(MSVC)
    target_compile_options(aitsnap PRIVATE /W4 /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aitsnap PRIVATE -Wall -Wextra -Wpedantic)
endif()
