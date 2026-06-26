cmake_minimum_required(VERSION 3.21)

project(ImageViewportInstallConsumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ImageViewport CONFIG REQUIRED)

if(NOT TARGET ImageViewport::ImageViewportplugin)
    message(FATAL_ERROR "ImageViewport package did not export ImageViewport::ImageViewportplugin")
endif()

if(NOT DEFINED IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT)
    message(FATAL_ERROR "IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT is required")
endif()

add_executable(imageviewport_install_consumer main.cpp)

target_compile_definitions(
    imageviewport_install_consumer
    PRIVATE IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT="${IMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT}")

target_link_libraries(
    imageviewport_install_consumer PRIVATE ImageViewport::ImageViewport
                                           ImageViewport::ImageViewportplugin Qt6::Qml Qt6::Quick)
