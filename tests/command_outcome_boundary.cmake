set(scan_root "${IMAGEVIEWPORT_SOURCE_DIR}/src")
set(allowed_files
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/imageviewportstate_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcommandoutcome_p.h"
    "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcommandoutcome.cpp")

file(
    GLOB_RECURSE scan_files
    LIST_DIRECTORIES false
    "${scan_root}/*.cpp"
    "${scan_root}/*.h")

set(violations)
foreach(scan_file IN LISTS scan_files)
    if(scan_file IN_LIST allowed_files)
        continue()
    endif()
    file(READ "${scan_file}" scan_content)

    set(forbidden_patterns
        "\\.setCommandDiagnostic[ \t\r\n]*\\("
        "\\.clearCommandDiagnosticForAcceptedCommand[ \t\r\n]*\\("
        "changes\\.commandRevision[ \t\r\n]*="
        "void[ \t\r\n]+setCommandDiagnostic[ \t\r\n]*\\("
        "void[ \t\r\n]+clearCommandDiagnosticForAcceptedCommand[ \t\r\n]*\\(")

    foreach(forbidden_pattern IN LISTS forbidden_patterns)
        if(scan_content MATCHES "${forbidden_pattern}")
            list(APPEND violations "${scan_file}: ${forbidden_pattern}")
        endif()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Command diagnostics and command revision effects must be owned by the command outcome helper boundary:\n  ${violation_report}"
    )
endif()
