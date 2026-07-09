set(controller_render_file "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerrender.cpp")

file(READ "${controller_render_file}" controller_render_content)

set(forbidden_patterns
    "ViewportRenderSnapshot[ \t\r\n]+[A-Za-z0-9_]+[ \t\r\n]*\\("
    "ViewportRenderLayer"
    "appendRenderLayer[ \t\r\n]*\\("
    "\\.imageLayers")

set(violations)
foreach(forbidden_pattern IN LISTS forbidden_patterns)
    if(controller_render_content MATCHES "${forbidden_pattern}")
        list(APPEND violations "${controller_render_file}: ${forbidden_pattern}")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Render snapshots and ordered render layers must be authored by the viewport engine, not the controller render adapter:\n  ${violation_report}"
    )
endif()
