# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

project(ImageViewport LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)
add_compile_definitions(QT_NO_KEYWORDS)

option(IMAGEVIEWPORT_BUILD_TESTS "Build ImageViewport tests" ON)

if(IMAGEVIEWPORT_BUILD_TESTS)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    enable_testing()
endif()

add_subdirectory(src)

if(IMAGEVIEWPORT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
