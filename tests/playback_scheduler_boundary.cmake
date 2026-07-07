set(scan_root "${IMAGEVIEWPORT_SOURCE_DIR}/src")
set(private_header "${scan_root}/imageviewport_p.h")
set(controller_file "${scan_root}/imageviewportcontroller.cpp")

file(READ "${private_header}" private_content)
file(READ "${controller_file}" controller_content)

set(violations)
if(private_content MATCHES "QTimer[ \t\r\n]+playbackTimer")
    list(APPEND violations "${private_header}: playback QTimer storage must live behind the scheduler")
endif()
if(private_content MATCHES "QElapsedTimer[ \t\r\n]+playbackClockTimebase")
    list(APPEND violations "${private_header}: playback elapsed timebase must live behind the scheduler")
endif()
if(private_content MATCHES "PlaybackClock[ \t\r\n]+playbackClock")
    list(APPEND violations "${private_header}: playback clock state must live behind the scheduler")
endif()
if(private_content MATCHES "(syncPlaybackTimer|stopPlaybackTimer|handlePlaybackTimer|takePlaybackTimerElapsed|flushPlaybackTimerElapsed)[ \t\r\n]*\\(")
    list(APPEND violations "${private_header}: playback timer entry points must be exposed through the scheduler")
endif()
if(controller_content MATCHES "ImageViewportPrivate::(syncPlaybackTimer|stopPlaybackTimer|handlePlaybackTimer|takePlaybackTimerElapsed|flushPlaybackTimerElapsed)[ \t\r\n]*\\(")
    list(APPEND violations "${controller_file}: playback timer entry points must be implemented by the scheduler")
endif()

if(violations)
    list(JOIN violations "\n  " violation_report)
    message(
        FATAL_ERROR
            "Playback timer state and callbacks must be isolated behind the item-side playback scheduler:\n  ${violation_report}"
    )
endif()
