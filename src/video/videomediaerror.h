// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEOMEDIAERROR_H
#define KIRIVIEW_VIDEOMEDIAERROR_H

#include <QString>

namespace kiriview {
enum class VideoMediaErrorCategory {
    Resource,
    Format,
    Network,
    AccessDenied,
    Unknown,
};

struct VideoMediaError
{
    VideoMediaErrorCategory category = VideoMediaErrorCategory::Unknown;
    int rawErrorCode = 0;
    QString diagnosticDetail;
};
}

#endif
