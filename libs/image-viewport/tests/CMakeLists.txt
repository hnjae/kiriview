# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

add_library(imageviewport_core_test_support INTERFACE)

target_link_libraries(imageviewport_core_test_support INTERFACE ImageViewport Qt6::Test)

target_include_directories(imageviewport_core_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(imageviewport_core_test_support
                           INTERFACE IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1)

add_library(imageviewport_test_support INTERFACE)

target_link_libraries(
    imageviewport_test_support INTERFACE imageviewport_core_test_support ImageViewportTestPlugin
                                         Qt6::Qml Qt6::Quick)

target_compile_definitions(imageviewport_test_support
                           INTERFACE IMAGEVIEWPORT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/src")

# Add a public-header isolation compile target.
function(add_imageviewport_public_header_compile_test target source)
    add_library(${target} OBJECT ${source})
    target_include_directories(${target} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Gui Qt6::Qml)
endfunction()

add_imageviewport_public_header_compile_test(imageviewport_types_header_compile
                                             public_headers/types.cpp)
add_imageviewport_public_header_compile_test(imageviewport_sequence_header_compile
                                             public_headers/sequence.cpp)
add_imageviewport_public_header_compile_test(imageviewport_provider_header_compile
                                             public_headers/provider.cpp)
add_imageviewport_public_header_compile_test(imageviewport_state_header_compile
                                             public_headers/state.cpp)
add_imageviewport_public_header_compile_test(imageviewport_umbrella_header_compile
                                             public_headers/umbrella.cpp)
add_library(imageviewport_viewport_header_compile OBJECT public_headers/viewport.cpp)
target_include_directories(imageviewport_viewport_header_compile PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(imageviewport_viewport_header_compile PRIVATE Qt6::Qml Qt6::Quick)

add_library(imageviewport_core_contract_header_compile OBJECT private_headers/core_contracts.cpp)
target_include_directories(imageviewport_core_contract_header_compile
                           PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(imageviewport_core_contract_header_compile PRIVATE ImageViewport)

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
add_imageviewport_core_qt_test(viewportengine tst_viewportengine.cpp)
add_imageviewport_core_qt_test(imageviewport_host_boundary tst_imageviewport_host_boundary.cpp)
add_imageviewport_core_qt_test(viewportproviderbridge_cleanup
                               tst_viewportproviderbridge_cleanup.cpp)
add_executable(viewportengine_capability_contract tst_viewportengine_capability_contract.cpp)
target_link_libraries(viewportengine_capability_contract PRIVATE ImageViewport)
target_compile_definitions(viewportengine_capability_contract
                           PRIVATE IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1)
add_test(NAME viewportengine_capability_contract COMMAND viewportengine_capability_contract)
add_imageviewport_qt_test(imageviewport_still tst_imageviewport_still.cpp)
add_imageviewport_qt_test(imageviewport_timed tst_imageviewport_timed.cpp)
add_imageviewport_qt_test(imageviewport_provider_contract tst_imageviewport_provider_contract.cpp)
add_imageviewport_qt_test(imageviewport_provider_lifecycle tst_imageviewport_provider_lifecycle.cpp)
add_imageviewport_qt_test(imageviewport_provider_metadata tst_imageviewport_provider_metadata.cpp)
add_imageviewport_qt_test(imageviewport_provider_requests tst_imageviewport_provider_requests.cpp)
add_imageviewport_qt_test(imageviewport_provider_refinement
                          tst_imageviewport_provider_refinement.cpp)
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
add_imageviewport_qt_test(source_ingress_contract tst_source_ingress_contract.cpp)
