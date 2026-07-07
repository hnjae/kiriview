set(provider_file "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerprovider.cpp")

file(READ "${provider_file}" provider_content)

set(forbidden_patterns
    "state\\.secondaryProvider"
    "\\.secondaryProviderState[ \t\r\n]*\\("
    "\\.secondaryActiveRequest"
    "\\.secondaryLatestNonPlaybackRequest")

set(violations)
foreach(forbidden_pattern IN LISTS forbidden_patterns)
    if(provider_content MATCHES "${forbidden_pattern}")
        list(APPEND violations "${provider_file}: ${forbidden_pattern}")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Provider controller code must use role-state views for direct secondary provider/request state access:\n  ${violation_report}"
    )
endif()
