set(scan_root "${IMAGEVIEWPORT_SOURCE_DIR}/src")
set(private_header "${scan_root}/imageviewport_p.h")
set(render_file "${scan_root}/imageviewportrender.cpp")

file(READ "${private_header}" private_content)
file(READ "${render_file}" render_content)

set(violations)
if(private_content MATCHES "RenderAdapter[ \t\r\n]+renderAdapter")
    list(APPEND violations "${private_header}: render adapter storage must live behind the render host")
endif()
if(private_content MATCHES "QSGNode\\*[ \t\r\n]+updatePaintNode[ \t\r\n]*\\(")
    list(APPEND violations "${private_header}: updatePaintNode must be exposed through the render host")
endif()
if(private_content MATCHES "geometryChanged[ \t\r\n]*\\(")
    list(APPEND violations "${private_header}: render geometry synchronization must be exposed through the render host")
endif()
if(render_content MATCHES "ImageViewportPrivate::updatePaintNode[ \t\r\n]*\\(")
    list(APPEND violations "${render_file}: updatePaintNode must be implemented by the render host")
endif()
if(render_content MATCHES "ImageViewportPrivate::geometryChanged[ \t\r\n]*\\(")
    list(APPEND violations "${render_file}: geometry synchronization must be implemented by the render host")
endif()
if(render_content MATCHES "ImageViewportProviderHost|providerHost|ViewportProviderBridge|applyFrameTransportEffect|openSession[ \t\r\n]*\\(")
    list(APPEND violations "${render_file}: render synchronization must not reach provider transport")
endif()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Render synchronization and scene graph resources must be isolated behind the item-side render host:\n  ${violation_report}"
    )
endif()
