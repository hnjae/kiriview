qt_add_library(ImageViewport STATIC)

qt_add_qml_module(ImageViewport
    URI ImageViewport
    VERSION 1.0
    OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/ImageViewport"
    SOURCES
        imageviewport.cpp
        imageviewport.h
)

target_include_directories(ImageViewport
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
)

target_link_libraries(ImageViewport
    PUBLIC
        Qt6::Qml
        Qt6::Quick
)
