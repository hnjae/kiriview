include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

qt_add_library(ImageViewport STATIC)

qt_add_qml_module(
    ImageViewport
    URI
    ImageViewport
    VERSION
    ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
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
    internalobservation_p.h
    imageviewport_p.cpp
    imageviewporttransactions.cpp
    imageviewportfacade.cpp
    imageviewportplaybackscheduler.cpp
    imageviewportplaybackscheduler_p.h
    imageviewportpresentation.cpp
    imageviewportproviderhost.cpp
    imageviewportproviderhost_p.h
    imageviewportrender.cpp
    imageviewportrenderhost_p.h
    imageviewportsnapshot.cpp
    imageviewport.cpp
    ImageViewport/ImageViewport
    ImageViewport/imageviewporttypes.h
    ImageViewport/imagesequence.h
    ImageViewport/imagesequenceprovider.h
    ImageViewport/imageviewportstate.h
    ImageViewport/imageviewport.h
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
    viewportengine.cpp
    viewportenginecoordinate.cpp
    viewportengineallocationoperations.cpp
    viewportengineallocationoperations_p.h
    viewportenginebuiltinframeoperations.cpp
    viewportenginebuiltinframeoperations_p.h
    viewportengineassignmentoperations.cpp
    viewportengineassignmentoperations_p.h
    viewportenginecapabilities_p.h
    viewportenginerender.cpp
    viewportenginerenderoperations.cpp
    viewportenginesnapshot.cpp
    viewportengineprojection.cpp
    viewportenginepresentationoperations_p.h
    viewportenginepresentation.cpp
    viewportengineplayback.cpp
    viewportengineplaybackoperations.cpp
    viewportengineplaybackoperations_p.h
    viewportengineprovider.cpp
    viewportengineprovidereventcompletionoperations.cpp
    viewportengineprovidereventcompletionoperations_p.h
    viewportengineproviderfailureoperations.cpp
    viewportengineproviderfailureoperations_p.h
    viewportengineproviderframeoperations.cpp
    viewportengineproviderframeoperations_p.h
    viewportengineprovidermetadataoperations.cpp
    viewportengineprovidermetadataoperations_p.h
    viewportengineproviderrequesttokenoperations.cpp
    viewportengineproviderrequestoperations.cpp
    viewportengineproviderrequestoperations_p.h
    viewportengineproviderprojection.cpp
    viewportengineprovidersessionoperations.cpp
    viewportengineprovidersessionoperations_p.h
    viewportengineproviderterminaloperations.cpp
    viewportengineproviderterminaloperations_p.h
    viewportengine_p.h
    viewportenginefacadecontract_p.h
    viewportenginestate_p.h
    viewportenginetargetspreadterminaloperations.cpp
    viewportenginetargetspreadterminaloperations_p.h
    viewportenginetargetspreadoperations.cpp
    viewportenginetargetspreadoperations_p.h
    viewportenginetestaccess_p.h
    viewportenginetransition_p.h
    viewportplaybackcontract_p.h
    viewportprovidercontract_p.h
    viewportrendercontract_p.h
    viewportitemtransaction_p.h
    viewportcommandoutcome.cpp
    viewportcommandoutcome_p.h
    viewportproviderbridge.cpp
    viewportproviderbridge_p.h)

target_include_directories(
    ImageViewport
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>" ${OPENGL_INCLUDE_DIR}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ImageViewport")

target_link_libraries(ImageViewport PUBLIC OpenGL::GL Qt6::Qml Qt6::Quick)

target_compile_options(ImageViewport
                       PUBLIC $<$<BOOL:${OPENGL_INCLUDE_DIR}>:-I${OPENGL_INCLUDE_DIR}>)

target_compile_definitions(
    ImageViewport
    PRIVATE
        $<$<BOOL:${IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES}>:IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1>)

set_target_properties(ImageViewport PROPERTIES EXPORT_NAME ImageViewport)

install(
    TARGETS ImageViewport
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ImageViewport)

install(
    FILES ImageViewport/ImageViewport ImageViewport/imageviewporttypes.h
          ImageViewport/imagesequence.h ImageViewport/imagesequenceprovider.h
          ImageViewport/imageviewportstate.h ImageViewport/imageviewport.h
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
