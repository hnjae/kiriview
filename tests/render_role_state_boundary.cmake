set(render_file "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerrender.cpp")

file(READ "${render_file}" render_content)

set(forbidden_patterns
    "state\\.secondaryProvider"
    "\\.secondaryDisplayedRequest"
    "\\.secondaryDisplayedImageSize"
    "\\.secondaryDisplayedImage"
    "\\.secondaryPendingRenderPayload")

set(violations)
foreach(forbidden_pattern IN LISTS forbidden_patterns)
    if(render_content MATCHES "${forbidden_pattern}")
        list(APPEND violations "${render_file}: ${forbidden_pattern}")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Render controller code must use display role-state views for direct secondary display/payload state access:\n  ${violation_report}"
    )
endif()
