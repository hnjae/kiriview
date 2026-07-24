# SPDX-FileCopyrightText: 2026 KIM Hyunjae
# SPDX-License-Identifier: AGPL-3.0-or-later

qt_add_library(VideoThumbnailExtraction STATIC)

target_sources(
    VideoThumbnailExtraction
    PRIVATE qtmultimediathumbnailbackend.cpp
            videothumbnailextraction.cpp
            videothumbnailextractionoperation.cpp
            videothumbnailextractionpolicy.cpp
            "${PROJECT_SOURCE_DIR}/private/videothumbnailextraction_p.h"
            VideoThumbnailExtraction/VideoThumbnailExtraction
            VideoThumbnailExtraction/videothumbnailextraction.h)

target_include_directories(
    VideoThumbnailExtraction
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>"
    PRIVATE "${PROJECT_SOURCE_DIR}/private")

target_link_libraries(
    VideoThumbnailExtraction
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE Qt6::Multimedia)

target_compile_features(VideoThumbnailExtraction PUBLIC cxx_std_23)
set_target_properties(VideoThumbnailExtraction PROPERTIES CXX_EXTENSIONS OFF)
