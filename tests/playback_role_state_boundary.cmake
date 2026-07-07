set(playback_file "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontrollerplayback.cpp")
set(controller_header "${IMAGEVIEWPORT_SOURCE_DIR}/src/viewportcontroller_p.h")

file(READ "${playback_file}" playback_content)
file(READ "${controller_header}" controller_header_content)

function(section_between content start_marker end_marker out_var)
    string(FIND "${content}" "${start_marker}" start_index)
    if(start_index EQUAL -1)
        message(FATAL_ERROR "Missing section start marker: ${start_marker}")
    endif()

    string(FIND "${content}" "${end_marker}" end_index)
    if(end_index EQUAL -1)
        message(FATAL_ERROR "Missing section end marker: ${end_marker}")
    endif()

    if(end_index LESS start_index)
        message(FATAL_ERROR "Section end marker appears before start marker: ${end_marker}")
    endif()

    math(EXPR section_length "${end_index} - ${start_index}")
    string(SUBSTRING "${content}" "${start_index}" "${section_length}" section_content)
    set("${out_var}" "${section_content}" PARENT_SCOPE)
endfunction()

function(append_violations scan_name scan_content)
    foreach(forbidden_pattern IN LISTS ARGN)
        if(scan_content MATCHES "${forbidden_pattern}")
            list(APPEND violations "${scan_name}: ${forbidden_pattern}")
        endif()
    endforeach()
    set(violations "${violations}" PARENT_SCOPE)
endfunction()

set(violations)

append_violations(
    "${controller_header}"
    "${controller_header_content}"
    "playSecondaryBuiltIn[ \t\r\n]*\\("
    "playSecondaryProvider[ \t\r\n]*\\(")

append_violations(
    "${playback_file}: full file"
    "${playback_content}"
    "applyPendingSecondaryProviderPlaybackTarget[ \t\r\n]*\\("
    "playSecondaryBuiltIn[ \t\r\n]*\\("
    "playSecondaryProvider[ \t\r\n]*\\(")

section_between(
    "${playback_content}"
    "ViewportCommandResult ViewportController::play(ImageViewport::PageRole role)"
    "ViewportCommandResult ViewportController::pause()"
    play_section)

append_violations(
    "${playback_file}: play(role)"
    "${play_section}"
    "viewportRequestState\\(viewport\\)\\.secondarySequenceIsProvider"
    "viewportRequestState\\(viewport\\)\\.secondaryActiveRequest"
    "state\\.secondaryProvider"
    "state\\.secondarySource"
    "\\.secondaryProviderTimedPlaybackCapability[ \t\r\n]*\\(")

section_between(
    "${playback_content}"
    "ViewportCommandResult ViewportController::stop(ImageViewport::PageRole role)"
    "ViewportCommandResult ViewportController::seek(int frame)"
    stop_section)

append_violations(
    "${playback_file}: stop(role)"
    "${stop_section}"
    "viewportRequestState\\(viewport\\)\\.secondaryLatestNonPlaybackRequest"
    "state\\.secondaryProvider"
    "setSecondaryActiveRequest[ \t\r\n]*\\(")

string(FIND "${playback_content}" "int ViewportController::playbackTimerInterval() const" timer_index)
if(timer_index EQUAL -1)
    message(FATAL_ERROR "Missing playbackTimerInterval section")
endif()
string(SUBSTRING "${playback_content}" "${timer_index}" -1 timing_and_advance_section)

append_violations(
    "${playback_file}: playback timer/advance"
    "${timing_and_advance_section}"
    "viewportRequestState\\(viewport\\)\\.secondaryActiveRequest"
    "state\\.secondaryProvider"
    "\\.hasSecondaryTimedSequence[ \t\r\n]*\\("
    "\\.secondarySequenceFrameCount[ \t\r\n]*\\("
    "\\.secondaryTotalDuration[ \t\r\n]*\\("
    "\\.secondarySequenceFrameStartPosition[ \t\r\n]*\\("
    "\\.secondarySequenceFrameIndexForPosition[ \t\r\n]*\\("
    "\\.secondarySequenceAuthoredAnimationFacts[ \t\r\n]*\\("
    "setSecondaryActiveRequest[ \t\r\n]*\\(")

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Playback controller code must use role-state and role-timing helpers for playback role access:\n  ${violation_report}"
    )
endif()
