// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEODOCUMENTPUBLICSIGNALS_H
#define KIRIVIEW_VIDEODOCUMENTPUBLICSIGNALS_H

#include "video/videodocumenttypes.h"

#include <functional>
#include <vector>

namespace kiriview {
enum class VideoDocumentPublicSignal {
    SessionSnapshot,
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
    PlaybackControlProjection,
};

struct VideoDocumentPublicSignalOperations
{
    std::function<void()> sessionSnapshotChanged;
    std::function<void()> sourceUrlChanged;
    std::function<void()> statusChanged;
    std::function<void()> errorStringChanged;
    std::function<void()> windowTitleFileNameChanged;
    std::function<void()> hasVideoChanged;
    std::function<void()> hasAudioChanged;
    std::function<void()> videoSizeChanged;
    std::function<void()> zoomPercentKnownChanged;
    std::function<void()> zoomPercentChanged;
    std::function<void()> videoOutputChanged;
    std::function<void()> embeddedMetadataChanged;
    std::function<void()> playbackControlProjectionChanged;
};

class VideoDocumentPublicSignalEmitter final
{
public:
    explicit VideoDocumentPublicSignalEmitter(VideoDocumentPublicSignalOperations operations);

    void emitSignal(VideoDocumentPublicSignal signal) const;

private:
    VideoDocumentPublicSignalOperations m_operations;
};

std::vector<VideoDocumentPublicSignal> videoDocumentPublicSignals(VideoDocumentChange change);
std::vector<VideoDocumentPublicSignal> videoDocumentPublicSignalsForChanges(
    const std::vector<VideoDocumentChange>& changes);
std::vector<VideoDocumentPublicSignal> videoDocumentPublicationSignalsForChanges(
    const std::vector<VideoDocumentChange>& changes);
}

#endif
