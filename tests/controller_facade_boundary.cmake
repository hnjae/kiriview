set(controller_header "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontroller_p.h")

file(READ "${controller_header}" controller_content)

string(FIND "${controller_content}" "class ViewportController" controller_start)
if(controller_start EQUAL -1)
    message(FATAL_ERROR "Missing ViewportController declaration")
endif()

string(FIND "${controller_content}" "#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES" test_probe_start)
if(test_probe_start EQUAL -1)
    message(FATAL_ERROR "Missing ViewportController test-probe boundary")
endif()

if(test_probe_start LESS controller_start)
    message(FATAL_ERROR "ViewportController test-probe boundary appears before declaration")
endif()

math(EXPR public_length "${test_probe_start} - ${controller_start}")
string(SUBSTRING "${controller_content}" "${controller_start}" "${public_length}" public_facade)

set(forbidden_public_helpers
    "publishLoadingWaitState[ \t\r\n]*\\("
    "initializeSecondaryActiveRequest[ \t\r\n]*\\("
    "stageBuiltInPrimarySpreadPayload[ \t\r\n]*\\("
    "publishUploadPendingState[ \t\r\n]*\\("
    "publishPendingRenderState[ \t\r\n]*\\("
    "publishSequenceReadyState[ \t\r\n]*\\("
    "publishStagedBuiltInPrimarySpreadReadyState[ \t\r\n]*\\("
    "rejectUnsupportedCommand[ \t\r\n]*\\("
    "rejectIgnoredNoRequestCommand[ \t\r\n]*\\("
    "seekSecondaryBuiltIn[ \t\r\n]*\\("
    "seekSecondaryProvider[ \t\r\n]*\\("
    "seekSecondaryProviderToPosition[ \t\r\n]*\\("
    "handleProviderWaitingEvent[ \t\r\n]*\\("
    "handleProviderWaiting[ \t\r\n]*\\("
    "handleProviderSessionClose[ \t\r\n]*\\("
    "allocateProviderRequestToken[ \t\r\n]*\\("
    "startProviderMetadataRequest[ \t\r\n]*\\("
    "queueProviderFrameRequest[ \t\r\n]*\\("
    "flushQueuedProviderFrameRequest[ \t\r\n]*\\(")

set(violations)
foreach(forbidden_pattern IN LISTS forbidden_public_helpers)
    if(public_facade MATCHES "${forbidden_pattern}")
        list(APPEND violations "${controller_header}: ${forbidden_pattern}")
    endif()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "ViewportController public facade must not expose domain-internal helper entry points:\n  ${violation_report}"
    )
endif()
