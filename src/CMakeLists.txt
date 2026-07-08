include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

qt_add_library(ImageViewport STATIC)

qt_add_qml_module(
    ImageViewport
    URI
    ImageViewport
    VERSION
    1.0
    OUTPUT_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/ImageViewport"
    SOURCES
    imagesequence.cpp
    imagesequencefactory.cpp
    imagesequencesource.cpp
    imagesequencesource_p.h
    imagesequenceprovider.cpp
    framepreparation.cpp
    framepreparation_p.h
    imageviewportdiagnostics.cpp
    imageviewportdiagnostics_p.h
    imageviewport_p.cpp
    imageviewportcontroller.cpp
    imageviewportfacade.cpp
    imageviewportplaybackscheduler.cpp
    imageviewportplaybackscheduler_p.h
    imageviewportpresentation.cpp
    imageviewportprovider.cpp
    imageviewportproviderhost.cpp
    imageviewportproviderhost_p.h
    imageviewportrender.cpp
    imageviewportrenderhost_p.h
    imageviewport.cpp
    imageviewport.h
    imageviewport_p.h
    imageviewportstate_p.h
    playbackclock_p.h
    playbacktimeline_p.h
    presentationgeometry.cpp
    presentationgeometry_p.h
    renderadapter.cpp
    renderadapter_p.h
    renderadapter_scenegraph_p.h
    renderfailurecause_p.h
    timingintervals.cpp
    timingintervals_p.h
    viewportcontroller.cpp
    viewportcontrollerassignmentcontract_p.h
    viewportcontrollercommandcontract_p.h
    viewportcontrollercorehelpers_p.h
    viewportcontrollergeometry.cpp
    viewportcontrollergeometryhelpers_p.h
    viewportcontrollerhelpers_p.h
    viewportcontrollermetadatacontract_p.h
    viewportcontrollerplaybackhelpers_p.h
    viewportcontrollerplaybackcontract_p.h
    viewportcontrollerprovidercontract_p.h
    viewportcontrollerrendercontract_p.h
    viewportcommandoutcome.cpp
    viewportcommandoutcome_p.h
    viewportcontrollermetadata.cpp
    viewportcontrollerplayback.cpp
    viewportcontrollerpresentation.cpp
    viewportcontrollerprovider.cpp
    viewportcontrollerproviderhelpers_p.h
    viewportcontrollerrender.cpp
    viewportcontrollerrenderhelpers_p.h
    viewportcontroller_p.h
    viewportproviderbridge.cpp
    viewportproviderbridge_p.h)

target_include_directories(
    ImageViewport
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport>" ${OPENGL_INCLUDE_DIR}
    PRIVATE)

target_link_libraries(ImageViewport PUBLIC OpenGL::GL Qt6::Qml Qt6::Quick)

target_compile_options(ImageViewport
                       PUBLIC $<$<BOOL:${OPENGL_INCLUDE_DIR}>:-I${OPENGL_INCLUDE_DIR}>)

target_compile_definitions(
    ImageViewport
    PRIVATE
        $<$<BOOL:${IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES}>:IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1>)

set_target_properties(ImageViewport PROPERTIES EXPORT_NAME ImageViewport)

set(imageviewport_installed_public_include_dir
    "${CMAKE_CURRENT_BINARY_DIR}/installed-public-include/ImageViewport")
set(imageviewport_installed_public_header
    "${imageviewport_installed_public_include_dir}/imageviewport.h")
add_custom_command(
    OUTPUT "${imageviewport_installed_public_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${imageviewport_installed_public_include_dir}"
    COMMAND
        "${CMAKE_COMMAND}" "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/imageviewport.h"
        "-DOUTPUT=${imageviewport_installed_public_header}" -P
        "${CMAKE_CURRENT_SOURCE_DIR}/generate_installed_public_header.cmake"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/imageviewport.h"
            "${CMAKE_CURRENT_SOURCE_DIR}/generate_installed_public_header.cmake"
    COMMENT "Generating sanitized installed ImageViewport public header"
    VERBATIM)
add_custom_target(
    ImageViewportInstalledPublicHeader ALL
    DEPENDS "${imageviewport_installed_public_header}"
    COMMENT "Preparing sanitized installed ImageViewport public header")

install(
    TARGETS ImageViewport
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport)

install(FILES "${imageviewport_installed_public_header}"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ImageViewport/qmldir
              ${CMAKE_CURRENT_BINARY_DIR}/ImageViewport/ImageViewport.qmltypes
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/qml/ImageViewport)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ImageViewport/${CMAKE_STATIC_LIBRARY_PREFIX}ImageViewportplugin${CMAKE_STATIC_LIBRARY_SUFFIX}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/qml/ImageViewport)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/ImageViewportConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageViewport
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR CMAKE_INSTALL_LIBDIR)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/ImageViewportConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImageViewport)
