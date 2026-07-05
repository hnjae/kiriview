set(boundary_files
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/imageviewportcontroller.cpp"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/imageviewport_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontroller.cpp"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontroller_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerrender.cpp")

set(forbidden_patterns
    "sequenceFrameImage[ \t\r\n]*\\("
    "secondarySequenceFrameImage[ \t\r\n]*\\("
    "sourceFrameImage[ \t\r\n]*\\("
    "ImageSequencePrivateAccess::frameImage[ \t\r\n]*\\(")

set(violations)
foreach(boundary_file IN LISTS boundary_files)
    file(READ "${boundary_file}" boundary_content)
    foreach(forbidden_pattern IN LISTS forbidden_patterns)
        if(boundary_content MATCHES "${forbidden_pattern}")
            list(APPEND violations "${boundary_file}: ${forbidden_pattern}")
        endif()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Built-in frames must cross controller/render boundaries as PreparedPayload values:\n  ${violation_report}"
    )
endif()
