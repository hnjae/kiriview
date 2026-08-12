# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

if(NOT NM OR NOT BINARY)
    message(FATAL_ERROR "NM and BINARY are required")
endif()

execute_process(
    COMMAND "${NM}" -C "${BINARY}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE symbols
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not inspect ${BINARY}: ${error}")
endif()

if(symbols
   MATCHES
   "ImageSequenceProvider(Session|Event|Request|Adapter|Descriptor|Metadata|FrameHandle|FailureHandle)"
)
    message(FATAL_ERROR "State-only subject acquired provider-session transport symbols")
endif()
