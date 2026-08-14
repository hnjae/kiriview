# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

project(ImageViewport LANGUAGES CXX)

set(imageviewport_public_header_names ImageViewport imagesequence.h imagesequenceprovider.h
                                      imageviewport.h imageviewportstate.h imageviewporttypes.h)
file(
    GLOB_RECURSE imageviewport_reachable_public_headers
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include/ImageViewport"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ImageViewport/*")
list(SORT imageviewport_public_header_names)
list(SORT imageviewport_reachable_public_headers)
if(NOT "${imageviewport_reachable_public_headers}" STREQUAL "${imageviewport_public_header_names}")
    message(
        FATAL_ERROR
            "The directly reachable ImageViewport public headers must match the canonical interface.\n"
            "Expected: ${imageviewport_public_header_names}\n"
            "Actual: ${imageviewport_reachable_public_headers}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Qml Quick)
qt_standard_project_setup(REQUIRES 6.11)
add_compile_definitions(QT_ENABLE_STRICT_MODE_UP_TO=0x060b00 QT_NO_KEYWORDS)

option(IMAGEVIEWPORT_BUILD_TESTS "Build ImageViewport tests" ON)

if(IMAGEVIEWPORT_BUILD_TESTS)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
    enable_testing()
endif()

add_subdirectory(src)

if(IMAGEVIEWPORT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
