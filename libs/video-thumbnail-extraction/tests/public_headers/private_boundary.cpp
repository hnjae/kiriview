// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <VideoThumbnailExtraction/videothumbnailextraction.h>

#if __has_include(<videothumbnailextraction_p.h>)
#error "The VideoThumbnailExtraction private header is visible to target consumers."
#endif

static_assert(kiriview::VideoThumbnailExtractionLimits::maximumOutputLongEdge > 0);
