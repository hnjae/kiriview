set(scan_roots "${IMAGEVIEWPORT_SOURCE_DIR}/src" "${IMAGEVIEWPORT_SOURCE_DIR}/tests")
set(disallowed_includes)

foreach(scan_root IN LISTS scan_roots)
    file(
        GLOB_RECURSE scan_files
        LIST_DIRECTORIES false
        "${scan_root}/*.cpp"
        "${scan_root}/*.h"
        "${scan_root}/*.cmake"
        "${scan_root}/CMakeLists.txt")
    foreach(scan_file IN LISTS scan_files)
        file(READ "${scan_file}" scan_content)
        if(scan_content MATCHES "#[ \t]*include[ \t]*\"imageviewporthelpers_p\\.h\"")
            list(APPEND disallowed_includes "${scan_file}")
        endif()
    endforeach()
endforeach()

if(disallowed_includes)
    list(JOIN disallowed_includes "\n  " include_report)
    message(
        FATAL_ERROR
            "Subsystems must include owner-specific helper headers instead of imageviewporthelpers_p.h:\n  ${include_report}"
    )
endif()

set(controller_helper_header "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerhelpers_p.h")
file(READ "${controller_helper_header}" controller_helper_content)
set(disallowed_controller_helpers)
set(disallowed_controller_helper_patterns
    "setCommandDiagnostic[ \t\r\n]*\\("
    "recordTargetSpreadTerminal[ \t\r\n]*\\("
    "acceptExplicitSeek[ \t\r\n]*\\("
    "applyPresentationTransition[ \t\r\n]*\\("
    "clearQueuedProviderFrameRequest[ \t\r\n]*\\("
    "beginAcceptedDisplayRequest[ \t\r\n]*\\("
    "discardPendingRenderCommit[ \t\r\n]*\\("
    "setSecondaryActiveRequest[ \t\r\n]*\\("
    "publishAcceptedTargetState[ \t\r\n]*\\("
    "publishProviderFrameLoadingState[ \t\r\n]*\\("
    "setPlaybackPhase[ \t\r\n]*\\("
    "armAuthoredAutoplayIfEligible[ \t\r\n]*\\("
    "applyPlaybackTarget[ \t\r\n]*\\(")

foreach(disallowed_pattern IN LISTS disallowed_controller_helper_patterns)
    if(controller_helper_content MATCHES "${disallowed_pattern}")
        list(APPEND disallowed_controller_helpers "${disallowed_pattern}")
    endif()
endforeach()

if(disallowed_controller_helpers)
    list(JOIN disallowed_controller_helpers "\n  " helper_report)
    message(
        FATAL_ERROR
            "viewportcontrollerhelpers_p.h must not own broad mutating controller transition helpers:\n  ${helper_report}"
    )
endif()
