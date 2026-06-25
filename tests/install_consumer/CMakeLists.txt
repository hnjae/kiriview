cmake_minimum_required(VERSION 3.21)

project(ImageViewportInstallConsumer
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ImageViewport CONFIG REQUIRED)

add_executable(imageviewport_install_consumer
    main.cpp
)

target_link_libraries(imageviewport_install_consumer
    PRIVATE
        ImageViewport::ImageViewport
)
