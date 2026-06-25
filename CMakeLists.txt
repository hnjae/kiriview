cmake_minimum_required(VERSION 3.21)

project(ImageViewport
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Qml Quick Test)
qt_standard_project_setup(REQUIRES 6.5)

option(IMAGEVIEWPORT_BUILD_EXAMPLES "Build ImageViewport examples" ON)
option(IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES "Enable private ImageViewport test probes" OFF)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)

if(IMAGEVIEWPORT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
