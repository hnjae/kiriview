set(item_boundary_files
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/imageviewportprovider.cpp"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/imageviewport_p.h")

set(item_forbidden_patterns
    "acceptsProviderSessionResult[ \t\r\n]*\\("
    "ImageSequenceProviderFrameHandle[^\n]*\\*")

set(item_violations)
foreach(boundary_file IN LISTS item_boundary_files)
    file(READ "${boundary_file}" boundary_content)
    foreach(forbidden_pattern IN LISTS item_forbidden_patterns)
        if(boundary_content MATCHES "${forbidden_pattern}")
            list(APPEND item_violations "${boundary_file}: ${forbidden_pattern}")
        endif()
    endforeach()
endforeach()

if(item_violations)
    list(JOIN item_violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Provider event admission, session filtering, and owned payload release must live at the controller event boundary:\n  ${violation_report}"
    )
endif()

file(READ "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportproviderevent_p.h" event_content)
if(NOT event_content MATCHES "quint64[ \t\r\n]+generation[ \t\r\n]*=")
    message(FATAL_ERROR "Normalized provider events must carry sequence generation identity")
endif()

file(READ "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportproviderbridge.cpp" bridge_content)
if(NOT bridge_content MATCHES "currentProviderGeneration[ \t\r\n]*\\(")
    message(FATAL_ERROR "Provider bridge callbacks must stamp normalized events with generation identity")
endif()

file(READ "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerprovider.cpp" controller_content)
if(NOT controller_content MATCHES "event\\.generation")
    message(FATAL_ERROR "Controller provider admission must validate normalized event generation identity")
endif()
