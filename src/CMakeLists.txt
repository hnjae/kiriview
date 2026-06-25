include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

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
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport>"
)

target_link_libraries(ImageViewport
    PUBLIC
        Qt6::Qml
        Qt6::Quick
)

target_compile_definitions(ImageViewport
    PRIVATE
        $<$<BOOL:${IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES}>:IMAGE_VIEWPORT_PRIVATE_TEST_PROBES=1>
)

set_target_properties(ImageViewport
    PROPERTIES
        EXPORT_NAME ImageViewport
)

install(TARGETS ImageViewport
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport
)

install(FILES imageviewport.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageViewportConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageViewport
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageViewport
)
