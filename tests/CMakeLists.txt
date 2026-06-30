add_library(imageviewport_test_support INTERFACE)

target_compile_definitions(ImageViewport PRIVATE IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1)

target_link_libraries(imageviewport_test_support INTERFACE ImageViewport ImageViewportplugin
                                                           Qt6::Qml Qt6::Quick Qt6::Test)

target_include_directories(imageviewport_test_support INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
                                                                ${OPENGL_INCLUDE_DIR})

target_compile_definitions(
    imageviewport_test_support INTERFACE IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1
                                         IMAGEVIEWPORT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/src")

# Add a Qt test executable with shared ImageViewport test wiring.
function(add_imageviewport_qt_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE imageviewport_test_support)
    qt_import_qml_plugins(${target})
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_imageviewport_qt_test(imageviewport_public_api tst_imageviewport_public_api.cpp)
add_imageviewport_qt_test(imagesequence_factory tst_imagesequence_factory.cpp)
add_imageviewport_qt_test(imageviewport_still tst_imageviewport_still.cpp)
add_imageviewport_qt_test(imageviewport_timed tst_imageviewport_timed.cpp)
add_imageviewport_qt_test(imageviewport_provider_contract tst_imageviewport_provider_contract.cpp)
add_imageviewport_qt_test(imageviewport_provider_lifecycle tst_imageviewport_provider_lifecycle.cpp)
add_imageviewport_qt_test(imageviewport_provider_metadata tst_imageviewport_provider_metadata.cpp)
add_imageviewport_qt_test(imageviewport_provider_requests tst_imageviewport_provider_requests.cpp)
add_imageviewport_qt_test(imageviewport_provider_frame_admission
                          tst_imageviewport_provider_frame_admission.cpp)
add_imageviewport_qt_test(imageviewport_provider_terminal tst_imageviewport_provider_terminal.cpp)
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
