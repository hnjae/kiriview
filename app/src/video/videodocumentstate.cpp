// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentstate.h"

#include "location/imageurl.h"

#include <algorithm>
#include <utility>

namespace {
QString fileNameForWindowTitle(const QUrl& sourceUrl)
{
    return kiriview::userVisibleFileNameForUrl(sourceUrl);
}

QSize normalizedVideoSize(QSize size)
{
    if (size.width() <= 0 || size.height() <= 0) {
        return {};
    }

    return size;
}

int nonNegative(int value) { return std::max(0, value); }
}

namespace kiriview {
VideoDocumentState::VideoDocumentState(ChangeCallback changeCallback)
    : m_changeCallback(std::move(changeCallback))
{
}

const QUrl& VideoDocumentState::sourceUrl() const { return m_sourceUrl; }

VideoDocumentStatus VideoDocumentState::status() const { return m_status; }

const QString& VideoDocumentState::errorString() const { return m_errorString; }

const std::optional<VideoSourceLoadFailure>& VideoDocumentState::sourceLoadFailure() const
{
    return m_sourceLoadFailure;
}

const std::optional<VideoBackendFailure>& VideoDocumentState::backendFailure() const
{
    return m_backendFailure;
}

const QString& VideoDocumentState::windowTitleFileName() const { return m_windowTitleFileName; }

bool VideoDocumentState::hasVideo() const { return m_hasVideo; }

bool VideoDocumentState::hasAudio() const { return m_hasAudio; }

QSize VideoDocumentState::videoSize() const { return m_videoSize; }

bool VideoDocumentState::zoomPercentKnown() const { return m_zoomPercentKnown; }

int VideoDocumentState::zoomPercent() const { return m_zoomPercent; }

const EmbeddedMetadata& VideoDocumentState::embeddedMetadata() const { return m_embeddedMetadata; }

void VideoDocumentState::resetForClearedSource()
{
    m_sourceLoadFailure.reset();
    m_backendFailure.reset();

    std::vector<VideoDocumentChange> changes;
    appendIfSourceUrlChanged(changes, QUrl());
    appendIfStatusChanged(changes, VideoDocumentStatus::Null);
    appendIfErrorStringChanged(changes, QString());
    appendIfWindowTitleFileNameChanged(changes, QString());
    appendIfHasVideoChanged(changes, false);
    appendIfHasAudioChanged(changes, false);
    appendIfVideoSizeChanged(changes, {});
    appendIfZoomPercentKnownChanged(changes, false);
    appendIfZoomPercentChanged(changes, 0);
    m_embeddedMetadata = {};
    changes.push_back(VideoDocumentChange::EmbeddedMetadata);
    publish(changes);
}

void VideoDocumentState::resetForSourceLoad(const QUrl& sourceUrl)
{
    m_sourceLoadFailure.reset();
    m_backendFailure.reset();

    std::vector<VideoDocumentChange> changes;
    appendIfSourceUrlChanged(changes, sourceUrl);
    appendIfWindowTitleFileNameChanged(changes, fileNameForWindowTitle(sourceUrl));
    appendIfErrorStringChanged(changes, QString());
    appendIfStatusChanged(changes, VideoDocumentStatus::Loading);
    appendIfHasVideoChanged(changes, false);
    appendIfHasAudioChanged(changes, false);
    appendIfVideoSizeChanged(changes, {});
    appendIfZoomPercentKnownChanged(changes, false);
    appendIfZoomPercentChanged(changes, 0);
    m_embeddedMetadata = {};
    changes.push_back(VideoDocumentChange::EmbeddedMetadata);
    publish(changes);
}

void VideoDocumentState::setSourceLoadFailure(VideoSourceLoadFailure failure)
{
    m_sourceLoadFailure = std::move(failure);
    m_backendFailure.reset();

    std::vector<VideoDocumentChange> changes;
    appendIfErrorStringChanged(changes, m_sourceLoadFailure->userMessage);
    appendIfStatusChanged(changes, VideoDocumentStatus::Error);
    publish(changes);
}

void VideoDocumentState::setBackendFailure(VideoBackendFailure failure)
{
    m_backendFailure = std::move(failure);
    m_sourceLoadFailure.reset();

    std::vector<VideoDocumentChange> changes;
    appendIfErrorStringChanged(changes, m_backendFailure->userMessage);
    appendIfStatusChanged(changes, VideoDocumentStatus::Error);
    publish(changes);
}

void VideoDocumentState::setStatusAndClearFailure(VideoDocumentStatus status)
{
    Q_ASSERT(status != VideoDocumentStatus::Error);
    if (status == VideoDocumentStatus::Error) {
        return;
    }
    m_sourceLoadFailure.reset();
    m_backendFailure.reset();

    std::vector<VideoDocumentChange> changes;
    appendIfErrorStringChanged(changes, QString());
    appendIfStatusChanged(changes, status);
    publish(changes);
}

void VideoDocumentState::setHasVideo(bool hasVideo)
{
    std::vector<VideoDocumentChange> changes;
    appendIfHasVideoChanged(changes, hasVideo);
    publish(changes);
}

void VideoDocumentState::setHasAudio(bool hasAudio)
{
    std::vector<VideoDocumentChange> changes;
    appendIfHasAudioChanged(changes, hasAudio);
    publish(changes);
}

void VideoDocumentState::setVideoSize(QSize size)
{
    std::vector<VideoDocumentChange> changes;
    appendIfVideoSizeChanged(changes, size);
    publish(changes);
}

void VideoDocumentState::setZoomPercent(std::optional<int> zoomPercent)
{
    std::vector<VideoDocumentChange> changes;
    appendZoomPercentChanges(changes, zoomPercent);
    publish(changes);
}

void VideoDocumentState::applyVideoOutputProjection(
    std::optional<int> zoomPercent, bool videoOutputChanged)
{
    std::vector<VideoDocumentChange> changes;
    appendZoomPercentChanges(changes, zoomPercent);
    if (videoOutputChanged) {
        changes.push_back(VideoDocumentChange::VideoOutput);
    }
    publish(changes);
}

void VideoDocumentState::appendZoomPercentChanges(
    std::vector<VideoDocumentChange>& changes, std::optional<int> zoomPercent)
{
    if (zoomPercent.has_value()) {
        appendIfZoomPercentChanged(changes, zoomPercent.value());
        appendIfZoomPercentKnownChanged(changes, true);
    } else {
        appendIfZoomPercentKnownChanged(changes, false);
        appendIfZoomPercentChanged(changes, 0);
    }
}

void VideoDocumentState::setEmbeddedMetadata(EmbeddedMetadata metadata)
{
    m_embeddedMetadata = std::move(metadata);
    publish(VideoDocumentChange::EmbeddedMetadata);
}

void VideoDocumentState::publish(VideoDocumentChange change)
{
    publish(std::vector<VideoDocumentChange> { change });
}

void VideoDocumentState::publish(const std::vector<VideoDocumentChange>& changes)
{
    if (changes.empty()) {
        return;
    }

    std::vector<VideoDocumentChange> uniqueChanges;
    for (VideoDocumentChange change : changes) {
        if (!std::ranges::contains(uniqueChanges, change)) {
            uniqueChanges.push_back(change);
        }
    }
    const ChangeCallback callback = m_changeCallback;
    if (callback) {
        callback(uniqueChanges);
    }
}

void VideoDocumentState::appendIfSourceUrlChanged(
    std::vector<VideoDocumentChange>& changes, const QUrl& sourceUrl)
{
    if (m_sourceUrl == sourceUrl) {
        return;
    }

    m_sourceUrl = sourceUrl;
    changes.push_back(VideoDocumentChange::SourceUrl);
}

void VideoDocumentState::appendIfStatusChanged(
    std::vector<VideoDocumentChange>& changes, VideoDocumentStatus status)
{
    if (m_status == status) {
        return;
    }

    m_status = status;
    changes.push_back(VideoDocumentChange::Status);
}

void VideoDocumentState::appendIfErrorStringChanged(
    std::vector<VideoDocumentChange>& changes, const QString& errorString)
{
    if (m_errorString == errorString) {
        return;
    }

    m_errorString = errorString;
    changes.push_back(VideoDocumentChange::ErrorString);
}

void VideoDocumentState::appendIfWindowTitleFileNameChanged(
    std::vector<VideoDocumentChange>& changes, const QString& fileName)
{
    if (m_windowTitleFileName == fileName) {
        return;
    }

    m_windowTitleFileName = fileName;
    changes.push_back(VideoDocumentChange::WindowTitleFileName);
}

void VideoDocumentState::appendIfHasVideoChanged(
    std::vector<VideoDocumentChange>& changes, bool hasVideo)
{
    if (m_hasVideo == hasVideo) {
        return;
    }

    m_hasVideo = hasVideo;
    changes.push_back(VideoDocumentChange::HasVideo);
}

void VideoDocumentState::appendIfHasAudioChanged(
    std::vector<VideoDocumentChange>& changes, bool hasAudio)
{
    if (m_hasAudio == hasAudio) {
        return;
    }

    m_hasAudio = hasAudio;
    changes.push_back(VideoDocumentChange::HasAudio);
}

void VideoDocumentState::appendIfVideoSizeChanged(
    std::vector<VideoDocumentChange>& changes, QSize size)
{
    const QSize normalizedSize = normalizedVideoSize(size);
    if (m_videoSize == normalizedSize) {
        return;
    }

    m_videoSize = normalizedSize;
    changes.push_back(VideoDocumentChange::VideoSize);
}

void VideoDocumentState::appendIfZoomPercentKnownChanged(
    std::vector<VideoDocumentChange>& changes, bool known)
{
    if (m_zoomPercentKnown == known) {
        return;
    }

    m_zoomPercentKnown = known;
    changes.push_back(VideoDocumentChange::ZoomPercentKnown);
}

void VideoDocumentState::appendIfZoomPercentChanged(
    std::vector<VideoDocumentChange>& changes, int zoomPercent)
{
    const int normalizedZoomPercent = nonNegative(zoomPercent);
    if (m_zoomPercent == normalizedZoomPercent) {
        return;
    }

    m_zoomPercent = normalizedZoomPercent;
    changes.push_back(VideoDocumentChange::ZoomPercent);
}

}
