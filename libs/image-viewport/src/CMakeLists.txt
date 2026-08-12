# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

qt_add_library(ImageViewportStateSubjects STATIC)

set(imageviewport_state_sources
    imagesequenceidentity.cpp ${PROJECT_SOURCE_DIR}/include/ImageViewport/imagesequenceidentity.h
    ${PROJECT_SOURCE_DIR}/include/ImageViewport/imageviewportstate.h
    ${PROJECT_SOURCE_DIR}/include/ImageViewport/imageviewporttypes.h)

target_sources(ImageViewportStateSubjects PRIVATE ${imageviewport_state_sources})
target_include_directories(ImageViewportStateSubjects
                           PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_link_libraries(ImageViewportStateSubjects PUBLIC Qt6::Core Qt6::Gui Qt6::QmlIntegration)
target_compile_features(ImageViewportStateSubjects PUBLIC cxx_std_23)
set_target_properties(ImageViewportStateSubjects PROPERTIES CXX_EXTENSIONS OFF)

qt_add_library(ImageViewportNonItemSubjects STATIC)

set(imageviewport_non_item_sources
    imagesequence.cpp
    imagesequence_p.h
    imagesequencefactory.cpp
    imagesequencesource.cpp
    imagesequencesource_p.h
    imagesequenceprovider.cpp
    framepreparation.cpp
    framepreparation_p.h
    imageviewportlimits_p.h
    imageviewportproviderfacts_p.h
    imageviewportprovidersubmission_p.h
    publicdiagnostic.cpp
    publicdiagnostic_p.h
    timingintervals.cpp
    timingintervals_p.h
    ${PROJECT_SOURCE_DIR}/include/ImageViewport/imagesequence.h
    ${PROJECT_SOURCE_DIR}/include/ImageViewport/imagesequenceprovider.h)

target_sources(ImageViewportNonItemSubjects PRIVATE ${imageviewport_non_item_sources})
target_include_directories(
    ImageViewportNonItemSubjects
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/include/ImageViewport")
target_link_libraries(ImageViewportNonItemSubjects PUBLIC ImageViewportStateSubjects Qt6::Core
                                                          Qt6::Gui Qt6::QmlIntegration)
target_compile_features(ImageViewportNonItemSubjects PUBLIC cxx_std_23)
set_target_properties(ImageViewportNonItemSubjects PROPERTIES CXX_EXTENSIONS OFF)
target_compile_definitions(
    ImageViewportNonItemSubjects
    PRIVATE $<$<BOOL:${IMAGEVIEWPORT_BUILD_TESTS}>:IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1>)

qt_add_library(ImageViewport STATIC)

set(imageviewport_sources
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
    imageviewportprovidersubmission_p.h
    imageviewportrender.cpp
    imageviewportrenderhost_p.h
    imageviewportsnapshot.cpp
    imageviewport.cpp
    imageviewportqmlforeign_p.h
    ${PROJECT_SOURCE_DIR}/include/ImageViewport/ImageViewport
    ${PROJECT_SOURCE_DIR}/include/ImageViewport/imageviewport.h
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
    viewportproviderbridge.cpp
    viewportproviderbridge_p.h)

if(IMAGEVIEWPORT_BUILD_TESTS)
    qt_add_qml_module(
        ImageViewport
        URI
        ImageViewportTest
        OUTPUT_DIRECTORY
        "${CMAKE_CURRENT_BINARY_DIR}/ImageViewportTest"
        PLUGIN_TARGET
        ImageViewportTestPlugin
        SOURCES
        ${imageviewport_sources})
    qt_generate_foreign_qml_types(ImageViewportStateSubjects ImageViewport)
    qt_generate_foreign_qml_types(ImageViewportNonItemSubjects ImageViewport)
else()
    target_sources(ImageViewport PRIVATE ${imageviewport_sources})
endif()

target_include_directories(
    ImageViewport
    PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}"
            "${PROJECT_SOURCE_DIR}/include/ImageViewport")

target_link_libraries(ImageViewport PUBLIC ImageViewportNonItemSubjects ImageViewportStateSubjects
                                           Qt6::Qml Qt6::Quick)
target_compile_features(ImageViewport PUBLIC cxx_std_23)
set_target_properties(ImageViewport PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(
    ImageViewport
    PRIVATE $<$<BOOL:${IMAGEVIEWPORT_BUILD_TESTS}>:IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1>)
