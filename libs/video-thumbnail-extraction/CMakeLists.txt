# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)

project(VideoThumbnailExtraction LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Multimedia)
qt_standard_project_setup(REQUIRES 6.11)
add_compile_definitions(QT_ENABLE_STRICT_MODE_UP_TO=0x060b00 QT_NO_KEYWORDS)

option(VIDEO_THUMBNAIL_EXTRACTION_BUILD_TESTS "Build VideoThumbnailExtraction tests" ON)

if(VIDEO_THUMBNAIL_EXTRACTION_BUILD_TESTS)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
    enable_testing()
endif()

add_subdirectory(src)

if(VIDEO_THUMBNAIL_EXTRACTION_BUILD_TESTS)
    add_subdirectory(tests)
endif()
