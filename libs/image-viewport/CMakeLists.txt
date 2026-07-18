# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

project(
    ImageViewport
    VERSION 1.0.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)
find_package(Qt6 6.5 REQUIRED COMPONENTS Qml Quick Test)
qt_standard_project_setup(REQUIRES 6.5)

option(IMAGEVIEWPORT_BUILD_EXAMPLES "Build ImageViewport examples" ON)
option(IMAGEVIEWPORT_BUILD_TESTS "Build ImageViewport tests" ON)
option(IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES "Enable private ImageViewport test probes"
       ${IMAGEVIEWPORT_BUILD_TESTS})

if(IMAGEVIEWPORT_BUILD_TESTS AND NOT IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES)
    set(IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES
        ON
        CACHE BOOL "Enable private ImageViewport test probes" FORCE)
endif()

if(IMAGEVIEWPORT_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(src)

if(IMAGEVIEWPORT_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(IMAGEVIEWPORT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
