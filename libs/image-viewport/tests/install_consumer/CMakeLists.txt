# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

project(ImageViewportInstallConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ImageViewport 1.0.0 EXACT CONFIG REQUIRED)

if(NOT DEFINED IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT)
    message(FATAL_ERROR "IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT is required")
endif()

add_executable(
    imageviewport_install_consumer
    main.cpp
    types.cpp
    sequence.cpp
    provider.cpp
    state.cpp
    viewport.cpp
    umbrella.cpp)

target_compile_definitions(
    imageviewport_install_consumer
    PRIVATE IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT="${IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT}")

target_link_libraries(imageviewport_install_consumer PRIVATE ImageViewport::ImageViewport)
