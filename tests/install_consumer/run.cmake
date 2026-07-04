set(prefix "${IMAGEVIEWPORT_TEST_DIR}/prefix")
set(consumer_build_dir "${IMAGEVIEWPORT_TEST_DIR}/build")
set(consumer_prefix_path "${prefix}")
if(DEFINED ENV{CMAKE_PREFIX_PATH} AND NOT "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
    string(REPLACE ":" ";" environment_prefix_path "$ENV{CMAKE_PREFIX_PATH}")
    list(APPEND consumer_prefix_path ${environment_prefix_path})
endif()
set(opengl_gl_library "${IMAGEVIEWPORT_OPENGL_GL_LIBRARY}")
if((NOT opengl_gl_library OR opengl_gl_library MATCHES "-NOTFOUND$")
   AND IMAGEVIEWPORT_OPENGL_OPENGL_LIBRARY)
    get_filename_component(opengl_library_dir "${IMAGEVIEWPORT_OPENGL_OPENGL_LIBRARY}" DIRECTORY)
    set(opengl_gl_library "${opengl_library_dir}/libGL.so")
endif()
file(REMOVE_RECURSE "${IMAGEVIEWPORT_TEST_DIR}")
file(MAKE_DIRECTORY "${prefix}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${IMAGEVIEWPORT_BUILD_DIR}" --prefix "${prefix}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "ImageViewport install failed:\n${install_output}\n${install_error}")
endif()

file(GLOB qml_module_dirs "${prefix}/qml/ImageViewport" "${prefix}/lib/qml/ImageViewport"
     "${prefix}/lib64/qml/ImageViewport")
list(LENGTH qml_module_dirs qml_module_dir_count)
if(qml_module_dir_count EQUAL 0)
    message(FATAL_ERROR "Installed ImageViewport package did not include a QML module directory")
endif()
list(GET qml_module_dirs 0 qml_module_dir)
get_filename_component(qml_import_root "${qml_module_dir}" DIRECTORY)
foreach(required_qml_file IN ITEMS qmldir ImageViewport.qmltypes libImageViewportplugin.a)
    if(NOT EXISTS "${qml_module_dir}/${required_qml_file}")
        message(FATAL_ERROR "Installed ImageViewport QML module is missing ${required_qml_file}")
    endif()
endforeach()

if(NOT EXISTS "${prefix}/include/ImageViewport/imageviewport.h")
    message(
        FATAL_ERROR
            "Installed ImageViewport package did not include the public imageviewport.h header")
endif()
file(READ "${prefix}/include/ImageViewport/imageviewport.h" installed_public_header)
if(installed_public_header MATCHES "ViewportProviderBridge")
    message(
        FATAL_ERROR "Installed ImageViewport public header exposed internal provider bridge type")
endif()
foreach(
    forbidden_header_token
    IN
    ITEMS
        IMAGEVIEWPORT_PRIVATE_TEST_PROBES
        ForTest
        ImageViewportInternal
        ImageSequenceData
        ImageSequencePrivateAccess
        ImageFramePrivateAccess
        QSGNode
        ViewportController
        RenderAdapter
        QSGTexture
        QRhi
        TimingModel
        m_provider
        m_timingIntervals
        m_frameImages
        [=[friend class ImageViewportPrivate]=])
    if(installed_public_header MATCHES "${forbidden_header_token}")
        message(
            FATAL_ERROR
                "Installed ImageViewport public header exposed forbidden token ${forbidden_header_token}"
        )
    endif()
endforeach()
file(GLOB installed_private_headers "${prefix}/include/ImageViewport/*_p.h")
if(installed_private_headers)
    message(
        FATAL_ERROR
            "Installed ImageViewport package exposed private headers: ${installed_private_headers}")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" -S "${IMAGEVIEWPORT_CONSUMER_SOURCE_DIR}" -B "${consumer_build_dir}"
        "-DCMAKE_PREFIX_PATH=${consumer_prefix_path}"
        -DOPENGL_INCLUDE_DIR=${IMAGEVIEWPORT_OPENGL_INCLUDE_DIR}
        -DOPENGL_opengl_LIBRARY=${IMAGEVIEWPORT_OPENGL_OPENGL_LIBRARY}
        -DOPENGL_glx_LIBRARY=${IMAGEVIEWPORT_OPENGL_GLX_LIBRARY}
        -DOPENGL_gl_LIBRARY=${opengl_gl_library} -DOpenGL_GL_PREFERENCE=LEGACY
        -DIMAGEVIEWPORT_INSTALLED_QML_IMPORT_ROOT=${qml_import_root}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error)
if(NOT configure_result EQUAL 0)
    message(
        FATAL_ERROR "Install consumer configure failed:\n${configure_output}\n${configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${consumer_build_dir}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "Install consumer build failed:\n${build_output}\n${build_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env QT_QPA_PLATFORM=offscreen
            "${consumer_build_dir}/imageviewport_install_consumer"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "Install consumer run failed:\n${run_output}\n${run_error}")
endif()
