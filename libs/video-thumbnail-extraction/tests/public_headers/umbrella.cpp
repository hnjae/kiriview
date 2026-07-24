// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <VideoThumbnailExtraction/VideoThumbnailExtraction>

#include <type_traits>

static_assert(std::is_enum_v<kiriview::VideoThumbnailExtractionFailureCause>);
static_assert(std::is_enum_v<kiriview::VideoThumbnailExtractionStatus>);
static_assert(std::is_move_constructible_v<kiriview::VideoThumbnailExtractionJob>);
