// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "videodocumentpublicsignals.h"

#include <algorithm>
#include <utility>

namespace {
template <typename Operation> void run(const Operation& operation)
{
    if (operation) {
        operation();
    }
}

bool affectsSessionSnapshot(kiriview::VideoDocumentPublicSignal signal)
{
    using Signal = kiriview::VideoDocumentPublicSignal;
    switch (signal) {
    case Signal::SessionSnapshot:
    case Signal::PlaybackControlProjection:
        return false;
    case Signal::SourceUrl:
    case Signal::Status:
    case Signal::ErrorString:
    case Signal::WindowTitleFileName:
    case Signal::HasVideo:
    case Signal::VideoSize:
    case Signal::ZoomPercentKnown:
    case Signal::ZoomPercent:
    case Signal::EmbeddedMetadata:
        return true;
    case Signal::HasAudio:
    case Signal::VideoOutput:
        return false;
    }

    return false;
}
}

namespace kiriview {
VideoDocumentPublicSignalEmitter::VideoDocumentPublicSignalEmitter(
    VideoDocumentPublicSignalOperations operations)
    : m_operations(std::move(operations))
{
}

void VideoDocumentPublicSignalEmitter::emitSignal(VideoDocumentPublicSignal signal) const
{
    switch (signal) {
    case VideoDocumentPublicSignal::SessionSnapshot:
        run(m_operations.sessionSnapshotChanged);
        return;
    case VideoDocumentPublicSignal::SourceUrl:
        run(m_operations.sourceUrlChanged);
        return;
    case VideoDocumentPublicSignal::Status:
        run(m_operations.statusChanged);
        return;
    case VideoDocumentPublicSignal::ErrorString:
        run(m_operations.errorStringChanged);
        return;
    case VideoDocumentPublicSignal::WindowTitleFileName:
        run(m_operations.windowTitleFileNameChanged);
        return;
    case VideoDocumentPublicSignal::HasVideo:
        run(m_operations.hasVideoChanged);
        return;
    case VideoDocumentPublicSignal::HasAudio:
        run(m_operations.hasAudioChanged);
        return;
    case VideoDocumentPublicSignal::VideoSize:
        run(m_operations.videoSizeChanged);
        return;
    case VideoDocumentPublicSignal::ZoomPercentKnown:
        run(m_operations.zoomPercentKnownChanged);
        return;
    case VideoDocumentPublicSignal::ZoomPercent:
        run(m_operations.zoomPercentChanged);
        return;
    case VideoDocumentPublicSignal::VideoOutput:
        run(m_operations.videoOutputChanged);
        return;
    case VideoDocumentPublicSignal::EmbeddedMetadata:
        run(m_operations.embeddedMetadataChanged);
        return;
    case VideoDocumentPublicSignal::PlaybackControlProjection:
        run(m_operations.playbackControlProjectionChanged);
        return;
    }
}

std::vector<VideoDocumentPublicSignal> videoDocumentPublicSignals(VideoDocumentChange change)
{
    switch (change) {
    case VideoDocumentChange::SourceUrl:
        return { VideoDocumentPublicSignal::SourceUrl };
    case VideoDocumentChange::Status:
        return { VideoDocumentPublicSignal::Status };
    case VideoDocumentChange::ErrorString:
        return { VideoDocumentPublicSignal::ErrorString };
    case VideoDocumentChange::WindowTitleFileName:
        return { VideoDocumentPublicSignal::WindowTitleFileName };
    case VideoDocumentChange::HasVideo:
        return { VideoDocumentPublicSignal::HasVideo };
    case VideoDocumentChange::HasAudio:
        return { VideoDocumentPublicSignal::HasAudio };
    case VideoDocumentChange::VideoSize:
        return { VideoDocumentPublicSignal::VideoSize };
    case VideoDocumentChange::ZoomPercentKnown:
        return { VideoDocumentPublicSignal::ZoomPercentKnown };
    case VideoDocumentChange::ZoomPercent:
        return { VideoDocumentPublicSignal::ZoomPercent };
    case VideoDocumentChange::VideoOutput:
        return { VideoDocumentPublicSignal::VideoOutput };
    case VideoDocumentChange::EmbeddedMetadata:
        return { VideoDocumentPublicSignal::EmbeddedMetadata };
    }

    return {};
}

std::vector<VideoDocumentPublicSignal> videoDocumentPublicSignalsForChanges(
    const std::vector<VideoDocumentChange>& changes)
{
    std::vector<VideoDocumentPublicSignal> plannedSignals;
    for (VideoDocumentChange change : changes) {
        for (VideoDocumentPublicSignal signal : videoDocumentPublicSignals(change)) {
            const bool alreadyPlanned = std::ranges::contains(plannedSignals, signal);
            if (!alreadyPlanned) {
                plannedSignals.push_back(signal);
            }
        }
    }
    return plannedSignals;
}

std::vector<VideoDocumentPublicSignal> videoDocumentPublicationSignalsForChanges(
    const std::vector<VideoDocumentChange>& changes)
{
    std::vector<VideoDocumentPublicSignal> plannedSignals
        = videoDocumentPublicSignalsForChanges(changes);
    if (std::ranges::any_of(plannedSignals, affectsSessionSnapshot)) {
        plannedSignals.insert(plannedSignals.begin(), VideoDocumentPublicSignal::SessionSnapshot);
    }
    return plannedSignals;
}
}
