// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentstatusplan.h"

namespace kiriview {
VideoDocumentStatusPlan videoDocumentStatusPlan(VideoDocumentStatusSnapshot snapshot)
{
    if (snapshot.sourceUrlEmpty) {
        return {};
    }
    if (snapshot.sourceLoadActive || !snapshot.mediaBackendAvailable) {
        return { VideoDocumentStatus::Loading, false, false };
    }
    const bool mediaEnded = snapshot.mediaStatus == VideoMediaStatus::EndOfMedia;
    VideoDocumentStatus status = VideoDocumentStatus::Loading;
    switch (snapshot.mediaStatus) {
    case VideoMediaStatus::Loaded:
    case VideoMediaStatus::Buffering:
    case VideoMediaStatus::Buffered:
    case VideoMediaStatus::EndOfMedia:
        status = VideoDocumentStatus::Ready;
        break;
    case VideoMediaStatus::Invalid:
        status = VideoDocumentStatus::Error;
        break;
    case VideoMediaStatus::Null:
    case VideoMediaStatus::Loading:
    case VideoMediaStatus::Stalled:
        break;
    }
    return { status, mediaEnded, mediaEnded };
}
}
