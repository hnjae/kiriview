// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_DISPLAYIMAGEQUALITY_H
#define KIRIVIEW_DISPLAYIMAGEQUALITY_H

namespace kiriview {
enum class DisplayImageQuality {
    Exact,
    FirstDisplay,
    ThumbnailPreview,
    BoundedDetail,
};

enum class DisplayImagePreviewOrigin {
    None,
    XdgThumbnail,
    RawEmbeddedThumbnail,
};

enum class DisplayImageRasterKind {
    ProvisionalPreview,
    AuthoritativeStill,
    TimedFrame,
    Refinement,
};
}

#endif
