# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

# Add a Qt test executable for component-owned contract coverage.
function(add_video_thumbnail_extraction_test target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE VideoThumbnailExtraction Qt6::Test)
    target_include_directories(${target} PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_library(video_thumbnail_extraction_subject_header_compile OBJECT public_headers/subject.cpp)
target_include_directories(video_thumbnail_extraction_subject_header_compile
                           PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(video_thumbnail_extraction_subject_header_compile PRIVATE Qt6::Core Qt6::Gui)

add_library(video_thumbnail_extraction_umbrella_header_compile OBJECT public_headers/umbrella.cpp)
target_include_directories(video_thumbnail_extraction_umbrella_header_compile
                           PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(video_thumbnail_extraction_umbrella_header_compile PRIVATE Qt6::Core Qt6::Gui)

add_video_thumbnail_extraction_test(video_thumbnail_extraction_contract
                                    tst_videothumbnailextraction_contract.cpp)
add_video_thumbnail_extraction_test(video_thumbnail_extraction_lifecycle
                                    tst_videothumbnailextraction_lifecycle.cpp)
