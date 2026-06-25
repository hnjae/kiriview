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
