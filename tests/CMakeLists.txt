add_library(imageviewport_core_test_support INTERFACE)

if(NOT IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES)
    message(
        FATAL_ERROR "IMAGEVIEWPORT_BUILD_TESTS requires IMAGE_VIEWPORT_ENABLE_PRIVATE_TEST_PROBES")
endif()

target_link_libraries(imageviewport_core_test_support INTERFACE ImageViewport Qt6::Test)

target_include_directories(imageviewport_core_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                                                                     ${OPENGL_INCLUDE_DIR})

add_library(imageviewport_test_support INTERFACE)

target_link_libraries(imageviewport_test_support INTERFACE imageviewport_core_test_support
                                                           ImageViewportplugin Qt6::Qml Qt6::Quick)

target_compile_definitions(
    imageviewport_test_support INTERFACE IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1
                                         IMAGEVIEWPORT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/src"
                                         IMAGEVIEWPORT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

# M0 baseline conformance anchors before controller ownership refactors: - retained display after
# failed replacement:
# imageviewport_provider_terminal/providerFrameFailureRetainsDisplayAndClearsOnSeek. - command
# failure preservation:
# imageviewport_provider_terminal/providerFrameFailureRetainsDisplayAndClearsOnSeek. -
# request/display revision behavior:
# imageviewport_public_api/emptyGeometryChangeIncrementsDisplayRevision. - provider stale results:
# imageviewport_provider_requests/providerTimedFrameSeekCancelsSupersededRequest. - render
# acknowledgement identity: imageviewport_render_commit/providerSupersededRenderFailureIsIgnored. -
# stop restoration: imageviewport_timed/timedFrameListStopWhileRenderWaitingRestoresPreviousDisplay.
# - metadata-bound target selection:
# imageviewport_provider_metadata/providerTimedMetadataSelectsInitialFrameRequest. - playback
# waiting: imageviewport_provider_playback/providerTimedPlaybackWaitsForMetadata.

# Add a Qt test executable without private probe declarations.
function(add_imageviewport_core_qt_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE imageviewport_core_test_support)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# Add a Qt test executable with private probe declarations and QML plugin imports.
function(add_imageviewport_qt_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE imageviewport_test_support)
    qt_import_qml_plugins(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_imageviewport_qt_test(imageviewport_public_api tst_imageviewport_public_api.cpp)
add_imageviewport_qt_test(imageviewport_public_api_commands
                          tst_imageviewport_public_api_commands.cpp)
add_imageviewport_qt_test(imageviewport_public_api_provider_roles
                          tst_imageviewport_public_api_provider_roles.cpp)
add_imageviewport_qt_test(imageviewport_public_api_qml tst_imageviewport_public_api_qml.cpp)
add_imageviewport_qt_test(imageviewport_state_snapshot tst_imageviewport_state_snapshot.cpp)
add_imageviewport_qt_test(imagesequence_factory tst_imagesequence_factory.cpp)
add_imageviewport_core_qt_test(playback_clock tst_playback_clock.cpp)
add_imageviewport_core_qt_test(playback_timeline tst_playback_timeline.cpp)
add_imageviewport_core_qt_test(viewportcontroller_playback tst_viewportcontroller_playback.cpp)
add_imageviewport_core_qt_test(viewportcontroller_presentation
                               tst_viewportcontroller_presentation.cpp)
add_imageviewport_core_qt_test(viewportcontroller_provider tst_viewportcontroller_provider.cpp)
add_imageviewport_qt_test(imageviewport_still tst_imageviewport_still.cpp)
add_imageviewport_qt_test(imageviewport_timed tst_imageviewport_timed.cpp)
add_imageviewport_qt_test(imageviewport_provider_contract tst_imageviewport_provider_contract.cpp)
add_imageviewport_qt_test(imageviewport_provider_lifecycle tst_imageviewport_provider_lifecycle.cpp)
add_imageviewport_qt_test(imageviewport_provider_metadata tst_imageviewport_provider_metadata.cpp)
add_imageviewport_qt_test(imageviewport_provider_requests tst_imageviewport_provider_requests.cpp)
add_imageviewport_qt_test(imageviewport_provider_frame_admission
                          tst_imageviewport_provider_frame_admission.cpp)
add_imageviewport_qt_test(imageviewport_provider_terminal tst_imageviewport_provider_terminal.cpp)
add_imageviewport_qt_test(imageviewport_provider_terminal_diagnostics
                          tst_imageviewport_provider_terminal_diagnostics.cpp)
add_imageviewport_qt_test(imageviewport_provider_terminal_playback
                          tst_imageviewport_provider_terminal_playback.cpp)
add_imageviewport_qt_test(imageviewport_provider_terminal_projection
                          tst_imageviewport_provider_terminal_projection.cpp)
add_imageviewport_qt_test(imageviewport_provider_terminal_recovery
                          tst_imageviewport_provider_terminal_recovery.cpp)
add_imageviewport_qt_test(imageviewport_provider_playback tst_imageviewport_provider_playback.cpp)
add_imageviewport_qt_test(imageviewport_render_scenegraph tst_imageviewport_render_scenegraph.cpp)
add_imageviewport_qt_test(imageviewport_render_commit tst_imageviewport_render_commit.cpp)
add_imageviewport_qt_test(imageviewport_presentation_state tst_imageviewport_presentation_state.cpp)

add_test(
    NAME imageviewport_install_consumer
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_BUILD_DIR=${CMAKE_BINARY_DIR}
        -DIMAGEVIEWPORT_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/install_consumer
        -DIMAGEVIEWPORT_TEST_DIR=${CMAKE_BINARY_DIR}/tests/install-consumer
        -DIMAGEVIEWPORT_OPENGL_INCLUDE_DIR=${OPENGL_INCLUDE_DIR}
        -DIMAGEVIEWPORT_OPENGL_OPENGL_LIBRARY=${OPENGL_opengl_LIBRARY}
        -DIMAGEVIEWPORT_OPENGL_GLX_LIBRARY=${OPENGL_glx_LIBRARY}
        -DIMAGEVIEWPORT_OPENGL_GL_LIBRARY=${OPENGL_gl_LIBRARY} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/install_consumer/run.cmake)

add_test(
    NAME structural::ownerSpecificHelperHeadersOnly
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/owner_specific_helper_headers_only.cmake)

add_test(
    NAME structural::builtInPayloadBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/built_in_payload_boundary.cmake)

add_test(
    NAME structural::providerEventAdmissionBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/provider_event_admission_boundary.cmake)

add_test(
    NAME structural::commandOutcomeBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/command_outcome_boundary.cmake)

add_test(
    NAME structural::mutationPrimitivesBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/mutation_primitives_boundary.cmake)

add_test(
    NAME structural::providerRoleStateBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/provider_role_state_boundary.cmake)

add_test(
    NAME structural::renderRoleStateBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/render_role_state_boundary.cmake)

add_test(
    NAME structural::playbackRoleStateBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/playback_role_state_boundary.cmake)

add_test(
    NAME structural::controllerHelperOwnershipBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/controller_helper_ownership_boundary.cmake)

add_test(
    NAME structural::controllerContractHeaderBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/controller_contract_header_boundary.cmake)

add_test(
    NAME structural::controllerFacadeBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/controller_facade_boundary.cmake)

add_test(
    NAME structural::providerHostBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/provider_host_boundary.cmake)

add_test(
    NAME structural::renderHostBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/render_host_boundary.cmake)

add_test(
    NAME structural::playbackSchedulerBoundary
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/playback_scheduler_boundary.cmake)

add_test(
    NAME structural::refactorBaselineInventory
    COMMAND
        ${CMAKE_COMMAND} -DIMAGEVIEWPORT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DIMAGEVIEWPORT_BUILD_DIR=${CMAKE_BINARY_DIR} -P
        ${CMAKE_CURRENT_SOURCE_DIR}/refactor_baseline_inventory.cmake)
