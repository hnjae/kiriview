set(scan_files
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontroller.cpp"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerprovider.cpp"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerplayback.cpp"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerrender.cpp")
set(core_helpers "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollercorehelpers_p.h")

file(READ "${core_helpers}" core_helpers_content)

set(required_helpers
    "markRequestMutation[ \t\r\n]*\\("
    "markDiagnosticsMutation[ \t\r\n]*\\("
    "publishRetainedOrEmptyDisplayStatus[ \t\r\n]*\\(")

set(violations)
foreach(required_helper IN LISTS required_helpers)
    if(NOT core_helpers_content MATCHES "${required_helper}")
        list(APPEND violations "${core_helpers}: missing ${required_helper}")
    endif()
endforeach()

foreach(scan_file IN LISTS scan_files)
    file(READ "${scan_file}" scan_content)
    foreach(forbidden_pattern
            "viewportDisplayState\\(viewport\\)\\.status[ \t\r\n]*=[ \t\r\n]*viewportDisplayState\\(viewport\\)\\.displayedImageSize\\.isValid"
            "changes\\.diagnostics[ \t\r\n]*=[ \t\r\n]*changes\\.diagnostics[ \t\r\n]*\\|\\|"
            "result\\.changes\\.diagnostics[ \t\r\n]*=[ \t\r\n]*result\\.changes\\.diagnostics[ \t\r\n]*\\|\\|")
        if(scan_content MATCHES "${forbidden_pattern}")
            list(APPEND violations "${scan_file}: ${forbidden_pattern}")
        endif()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Controller mutation code must use centralized wait-state and diagnostics primitives:\n  ${violation_report}"
    )
endif()
