// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEODOCUMENTTYPES_H
#define KIRIVIEW_VIDEODOCUMENTTYPES_H

#include <QString>
#include <QUrl>

namespace kiriview {
enum class VideoDocumentStatus {
    Null,
    Loading,
    Ready,
    Error,
};

enum class VideoDocumentChange {
    SourceUrl,
    Status,
    ErrorString,
    WindowTitleFileName,
    HasVideo,
    HasAudio,
    VideoSize,
    ZoomPercentKnown,
    ZoomPercent,
    VideoOutput,
    EmbeddedMetadata,
};

enum class VideoSourceLoadFailureKind {
    PlaybackUrlResolution,
    PlaybackBackendCreation,
};

enum class VideoSourceLoadFailureSeverity {
    Error,
};

struct VideoSourceLoadFailure
{
    QUrl sourceUrl;
    VideoSourceLoadFailureKind kind = VideoSourceLoadFailureKind::PlaybackUrlResolution;
    QString userMessage;
    QString diagnosticDetail;
    VideoSourceLoadFailureSeverity severity = VideoSourceLoadFailureSeverity::Error;
    bool retryable = false;
};
}

#endif
