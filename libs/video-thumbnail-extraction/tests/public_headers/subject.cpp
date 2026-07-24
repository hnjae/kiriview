// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <VideoThumbnailExtraction/videothumbnailextraction.h>

#include <type_traits>

static_assert(kiriview::VideoThumbnailExtractionLimits::maximumOutputLongEdge == 4096);
static_assert(kiriview::VideoThumbnailExtractionLimits::maximumOutputBytes == 67'108'864);
static_assert(kiriview::VideoThumbnailExtractionLimits::maximumDiagnosticCharacters == 1024);
static_assert(!std::is_copy_constructible_v<kiriview::VideoThumbnailExtractionJob>);
static_assert(!std::is_copy_assignable_v<kiriview::VideoThumbnailExtractionJob>);
static_assert(std::is_nothrow_move_constructible_v<kiriview::VideoThumbnailExtractionJob>);
static_assert(std::is_nothrow_move_assignable_v<kiriview::VideoThumbnailExtractionJob>);
