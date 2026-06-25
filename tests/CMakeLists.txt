add_executable(tst_imageviewport
    tst_imageviewport.cpp
)

target_link_libraries(tst_imageviewport
    PRIVATE
        ImageViewport
        ImageViewportplugin
        Qt6::Qml
        Qt6::Quick
        Qt6::Test
)

target_include_directories(tst_imageviewport
    PRIVATE
        ${OPENGL_INCLUDE_DIR}
)

target_compile_definitions(tst_imageviewport
    PRIVATE
        IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1
        IMAGEVIEWPORT_QML_IMPORT_PATH="${CMAKE_BINARY_DIR}/src"
)

target_compile_definitions(ImageViewport
    PRIVATE
        IMAGEVIEWPORT_PRIVATE_TEST_PROBES=1
)

qt_import_qml_plugins(tst_imageviewport)

add_test(NAME imageviewport COMMAND tst_imageviewport)
set_tests_properties(imageviewport
    PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

add_test(NAME imageviewport_install_consumer
    COMMAND
        ${CMAKE_COMMAND}
            -DIMAGEVIEWPORT_BUILD_DIR=${CMAKE_BINARY_DIR}
            -DIMAGEVIEWPORT_CONSUMER_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/install_consumer
            -DIMAGEVIEWPORT_TEST_DIR=${CMAKE_BINARY_DIR}/tests/install-consumer
            -DIMAGEVIEWPORT_OPENGL_INCLUDE_DIR=${OPENGL_INCLUDE_DIR}
            -DIMAGEVIEWPORT_OPENGL_OPENGL_LIBRARY=${OPENGL_opengl_LIBRARY}
            -DIMAGEVIEWPORT_OPENGL_GLX_LIBRARY=${OPENGL_glx_LIBRARY}
            -DIMAGEVIEWPORT_OPENGL_GL_LIBRARY=${OPENGL_gl_LIBRARY}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/install_consumer/run.cmake
)
