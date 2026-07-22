// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentruntime.h"

#include "localization/imageerrortext.h"
#include "metadata/embeddedmetadata.h"
#include "video/videodocumentstatusplan.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QObject>
#include <utility>
#include <variant>

Q_LOGGING_CATEGORY(kiriviewVideoLog, "org.hnjae.kiriview.video", QtWarningMsg)

namespace {
const char* videoMediaErrorCategoryName(kiriview::VideoMediaErrorCategory category)
{
    switch (category) {
    case kiriview::VideoMediaErrorCategory::Resource:
        return "Resource";
    case kiriview::VideoMediaErrorCategory::Format:
        return "Format";
    case kiriview::VideoMediaErrorCategory::Network:
        return "Network";
    case kiriview::VideoMediaErrorCategory::AccessDenied:
        return "AccessDenied";
    case kiriview::VideoMediaErrorCategory::Unknown:
        return "Unknown";
    }

    return "Unknown";
}
}

namespace kiriview {
VideoDocumentRuntime::VideoDocumentRuntime(QObject* documentObject, ChangeCallback changeCallback,
    std::unique_ptr<VideoPlaybackUrlResolver> playbackUrlResolver,
    MediaBackendFactory mediaBackendFactory, TimerScheduler playbackControlTimerScheduler,
    VideoPlaybackControlProjectionCallback playbackControlProjectionCallback)
    : m_documentObject(documentObject)
    , m_state(std::move(changeCallback))
    , m_playbackControls(documentObject, std::move(playbackControlTimerScheduler),
          std::move(playbackControlProjectionCallback))
    , m_mediaBackendFactory(std::move(mediaBackendFactory))
    , m_sourceLoadRuntime(std::move(playbackUrlResolver))
    , m_outputRuntime(documentObject,
          VideoOutputRuntimeCallbacks {
              [this](QObject* videoOutput) {
                  if (m_mediaBackend != nullptr) {
                      m_mediaBackend->setVideoOutput(videoOutput);
                  }
              },
              [this]() { publish(VideoDocumentChange::VideoOutput); },
              [this]() { updateZoomPercent(); },
          })
{
    if (!m_mediaBackendFactory) {
        m_mediaBackendFactory = []() { return createDefaultVideoMediaBackend(); };
    }
}

VideoMediaBackend* VideoDocumentRuntime::replaceMediaBackendForSource(const QUrl& publicSourceUrl)
{
    clearPlaybackSource();
    m_mediaBackend = m_mediaBackendFactory();
    if (m_mediaBackend == nullptr) {
        return nullptr;
    }

    const PlaybackLifecycle lifecycle = acceptPlaybackCallbacks(publicSourceUrl);
    installMediaBackendCallbacks(lifecycle);
    m_mediaBackend->setMuted(m_playbackControls.mediaSnapshot().muted);
    if (m_outputRuntime.videoOutput() != nullptr) {
        m_mediaBackend->setVideoOutput(m_outputRuntime.videoOutput());
    }

    return m_mediaBackend.get();
}

void VideoDocumentRuntime::installMediaBackendCallbacks(const PlaybackLifecycle& lifecycle)
{
    m_mediaBackend->setCallbacks(VideoMediaBackendCallbacks {
        [this, lifecycle]() { updateStatusFromBackend(lifecycle); },
        [this, lifecycle](
            VideoMediaError error) { updateErrorFromBackend(lifecycle, std::move(error)); },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifecycle]() {
            if (!playbackCallbacksAccepted(lifecycle)) {
                return;
            }
            m_state.setHasVideo(m_mediaBackend->hasVideo());
            refreshPlaybackControlsFromBackend(lifecycle);
            updateZoomPercent();
        },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                m_state.setHasAudio(m_mediaBackend->hasAudio());
            }
        },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                m_state.setVideoSize(m_mediaBackend->videoSize());
            }
        },
        [this, lifecycle]() {
            if (playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        {},
    });
}

VideoDocumentRuntime::~VideoDocumentRuntime()
{
    m_sourceLoadRuntime.shutdown();
    clearPlaybackSource();
}

QUrl VideoDocumentRuntime::sourceUrl() const { return m_state.sourceUrl(); }

void VideoDocumentRuntime::setSourceUrl(const QUrl& sourceUrl)
{
    if (m_state.sourceUrl() == sourceUrl && m_playbackSourceDevice.device == nullptr) {
        return;
    }

    m_sourceLoadRuntime.setSourceUrl(sourceUrl, m_documentObject,
        [this](VideoSourceLoadPlan plan) { executeSourceLoadPlan(plan); });
}

void VideoDocumentRuntime::setSourceDevice(
    const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice)
{
    if (sourceUrl.isEmpty() || sourceDevice.device == nullptr) {
        setSourceUrl(QUrl());
        return;
    }

    m_sourceLoadRuntime.cancelPendingResolution();
    executeSourceLoadPlan(videoSourceLoadStartPlan(sourceUrl));
    applyPlaybackSourceDevice(std::move(sourceDevice), sourceUrl);
}

VideoDocumentStatus VideoDocumentRuntime::status() const { return m_state.status(); }

QString VideoDocumentRuntime::errorString() const { return m_state.errorString(); }

const std::optional<VideoSourceLoadFailure>& VideoDocumentRuntime::sourceLoadFailure() const
{
    return m_state.sourceLoadFailure();
}

const std::optional<VideoBackendFailure>& VideoDocumentRuntime::backendFailure() const
{
    return m_state.backendFailure();
}

QString VideoDocumentRuntime::windowTitleFileName() const { return m_state.windowTitleFileName(); }

qint64 VideoDocumentRuntime::duration() const
{
    return m_playbackControls.mediaSnapshot().durationMsec;
}

qint64 VideoDocumentRuntime::position() const
{
    return m_playbackControls.mediaSnapshot().positionMsec;
}

void VideoDocumentRuntime::setPosition(qint64 position)
{
    executePlaybackControlPlan(videoPlaybackSetPositionPlan(playbackControlSnapshot(), position));
}

bool VideoDocumentRuntime::playing() const { return m_playbackControls.mediaSnapshot().playing; }

bool VideoDocumentRuntime::seekable() const { return m_playbackControls.mediaSnapshot().seekable; }

bool VideoDocumentRuntime::hasVideo() const { return m_state.hasVideo(); }

bool VideoDocumentRuntime::hasAudio() const { return m_state.hasAudio(); }

QSize VideoDocumentRuntime::videoSize() const { return m_state.videoSize(); }

bool VideoDocumentRuntime::zoomPercentKnown() const { return m_state.zoomPercentKnown(); }

int VideoDocumentRuntime::zoomPercent() const { return m_state.zoomPercent(); }

bool VideoDocumentRuntime::muted() const { return m_playbackControls.mediaSnapshot().muted; }

void VideoDocumentRuntime::setMuted(bool muted)
{
    VideoPlaybackControlMediaSnapshot snapshot = m_playbackControls.mediaSnapshot();
    snapshot.muted = muted;
    m_playbackControls.acceptMediaSnapshot(snapshot);
    if (m_mediaBackend != nullptr) {
        m_mediaBackend->setMuted(muted);
    }
}

QObject* VideoDocumentRuntime::videoOutput() const { return m_outputRuntime.videoOutput(); }

const EmbeddedMetadata& VideoDocumentRuntime::embeddedMetadata() const
{
    return m_state.embeddedMetadata();
}

void VideoDocumentRuntime::setVideoOutput(QObject* videoOutput)
{
    m_outputRuntime.setVideoOutput(videoOutput);
}

void VideoDocumentRuntime::setVideoOutputGeometry(
    const QRectF& contentRect, const QRectF& sourceRect)
{
    m_outputRuntime.setVideoOutputGeometry(contentRect, sourceRect);
}

void VideoDocumentRuntime::play()
{
    executePlaybackControlPlan(videoPlaybackPlayPlan(playbackControlSnapshot()));
}

void VideoDocumentRuntime::pause()
{
    executePlaybackControlPlan(videoPlaybackPausePlan(playbackControlSnapshot()));
}

void VideoDocumentRuntime::stop()
{
    executePlaybackControlPlan(videoPlaybackStopPlan(playbackControlSnapshot()));
}

void VideoDocumentRuntime::togglePlayback()
{
    executePlaybackControlPlan(videoPlaybackTogglePlan(playbackControlSnapshot()));
}

void VideoDocumentRuntime::toggleMuted() { setMuted(!muted()); }

void VideoDocumentRuntime::seekBy(qint64 deltaMilliseconds)
{
    executePlaybackControlPlan(
        videoPlaybackSeekByPlan(playbackControlSnapshot(), deltaMilliseconds));
}

const VideoPlaybackControlProjection& VideoDocumentRuntime::playbackControlProjection() const
{
    return m_playbackControls.projection();
}

void VideoDocumentRuntime::reportPlaybackControlEnvironment(
    VideoPlaybackControlEnvironment environment)
{
    m_playbackControls.acceptEnvironment(environment);
}

void VideoDocumentRuntime::reportPlaybackControlInteraction(bool active)
{
    m_playbackControls.setInteractionActive(active);
}

void VideoDocumentRuntime::revealPlaybackControls() { m_playbackControls.reveal(); }

void VideoDocumentRuntime::beginPlaybackScrub() { m_playbackControls.beginScrub(); }

void VideoDocumentRuntime::updatePlaybackScrub(qint64 positionMsec)
{
    m_playbackControls.updateScrub(positionMsec);
}

void VideoDocumentRuntime::commitPlaybackScrub()
{
    const std::optional<qint64> positionMsec = m_playbackControls.commitScrub();
    if (positionMsec.has_value()) {
        setPosition(positionMsec.value());
    }
}

void VideoDocumentRuntime::cancelPlaybackScrub() { m_playbackControls.cancelScrub(); }

void VideoDocumentRuntime::requestPlaybackControlSeek(qint64 positionMsec)
{
    const std::optional<qint64> acceptedPosition = m_playbackControls.requestSeek(positionMsec);
    if (acceptedPosition.has_value()) {
        setPosition(acceptedPosition.value());
    }
}

VideoPlaybackControlSnapshot VideoDocumentRuntime::playbackControlSnapshot() const
{
    const VideoPlaybackControlMediaSnapshot& media = m_playbackControls.mediaSnapshot();
    return VideoPlaybackControlSnapshot {
        m_state.sourceUrl().isEmpty(),
        m_mediaBackend != nullptr,
        media.playing,
        media.mediaEnded,
        media.seekable,
        media.positionMsec,
        media.durationMsec,
    };
}

void VideoDocumentRuntime::executePlaybackControlPlan(const VideoPlaybackControlPlan& plan)
{
    for (const VideoPlaybackBackendOperation& operation : plan.backendOperations) {
        executePlaybackBackendOperation(operation);
    }

    applyPlaybackStateDelta(plan.stateDelta);
}

void VideoDocumentRuntime::executePlaybackBackendOperation(VideoPlaybackBackendOperation operation)
{
    std::visit(
        [this](const auto& payload) { executePlaybackBackendOperation(payload); }, operation);
}

void VideoDocumentRuntime::executePlaybackBackendOperation(EnsureVideoPlaybackBackendOperation)
{
    // Backends are created only when a resolved source lifecycle is accepted.
}

void VideoDocumentRuntime::executePlaybackBackendOperation(PlayVideoPlaybackOperation)
{
    if (m_mediaBackend != nullptr) {
        m_mediaBackend->play();
    }
}

void VideoDocumentRuntime::executePlaybackBackendOperation(PauseVideoPlaybackOperation)
{
    if (m_mediaBackend != nullptr) {
        m_mediaBackend->pause();
    }
}

void VideoDocumentRuntime::executePlaybackBackendOperation(StopVideoPlaybackOperation)
{
    if (m_mediaBackend != nullptr) {
        m_mediaBackend->stop();
    }
}

void VideoDocumentRuntime::executePlaybackBackendOperation(
    SetVideoPlaybackPositionOperation operation)
{
    if (m_mediaBackend != nullptr) {
        m_mediaBackend->setPosition(operation.position);
    }
}

void VideoDocumentRuntime::applyPlaybackStateDelta(const VideoPlaybackStateDelta& delta)
{
    VideoPlaybackControlMediaSnapshot snapshot = m_playbackControls.mediaSnapshot();
    if (delta.mediaEnded.has_value()) {
        snapshot.mediaEnded = delta.mediaEnded.value();
    }
    if (delta.playing.has_value()) {
        snapshot.playing = delta.playing.value();
    }
    if (delta.position.has_value()) {
        snapshot.positionMsec = delta.position.value();
    }
    m_playbackControls.acceptMediaSnapshot(snapshot);
}

void VideoDocumentRuntime::clearPlaybackSource()
{
    invalidatePlaybackCallbacks();
    if (m_mediaBackend != nullptr) {
        m_mediaBackend->stop();
        m_mediaBackend->setSource(QUrl());
        m_mediaBackend->setVideoOutput(nullptr);
        m_mediaBackend->setCallbacks({});
    }
    m_mediaBackend.reset();
    m_playbackSourceDevice = {};
}

void VideoDocumentRuntime::executeSourceLoadPlan(const VideoSourceLoadPlan& plan)
{
    for (const VideoSourceLoadOperation& operation : plan) {
        executeSourceLoadOperation(operation);
    }
}

void VideoDocumentRuntime::executeSourceLoadOperation(const VideoSourceLoadOperation& operation)
{
    std::visit([this](const auto& payload) { executeSourceLoadOperation(payload); }, operation);
}

void VideoDocumentRuntime::executeSourceLoadOperation(ClearVideoPlaybackSourceOperation)
{
    clearPlaybackSource();
}

void VideoDocumentRuntime::executeSourceLoadOperation(ResetClearedVideoSourceOperation)
{
    replacePlaybackControlSource();
    m_state.resetForClearedSource();
}

void VideoDocumentRuntime::executeSourceLoadOperation(
    const ResetVideoSourceLoadOperation& operation)
{
    invalidatePlaybackCallbacks();
    replacePlaybackControlSource();
    m_state.resetForSourceLoad(operation.sourceUrl);
}

void VideoDocumentRuntime::executeSourceLoadOperation(
    const ApplyVideoPlaybackUrlOperation& operation)
{
    applyResolvedPlaybackUrl(operation.playbackUrl);
}

void VideoDocumentRuntime::executeSourceLoadOperation(
    const PublishVideoSourceLoadFailureOperation& operation)
{
    publishSourceLoadFailure(operation.failure);
}

void VideoDocumentRuntime::applyResolvedPlaybackUrl(const QUrl& playbackUrl)
{
    VideoMediaBackend* mediaBackend = replaceMediaBackendForSource(m_state.sourceUrl());
    if (mediaBackend == nullptr) {
        return;
    }
    mediaBackend->setSource(playbackUrl);
    m_playbackSourceDevice = {};
    m_state.setEmbeddedMetadata(playbackUrl.isLocalFile()
            ? parsePathEmbeddedMetadata(playbackUrl.toLocalFile())
            : EmbeddedMetadata {});
    m_state.setVideoSize(mediaBackend->videoSize());
    updateStatusFromBackend(*m_activePlaybackLifecycle);
    play();
}

void VideoDocumentRuntime::applyPlaybackSourceDevice(
    VideoPlaybackSourceDevice sourceDevice, const QUrl& sourceUrl)
{
    VideoMediaBackend* mediaBackend = replaceMediaBackendForSource(sourceUrl);
    if (mediaBackend == nullptr) {
        return;
    }
    m_playbackSourceDevice = std::move(sourceDevice);
    mediaBackend->setSourceDevice(m_playbackSourceDevice.device.get(), sourceUrl);
    m_state.setVideoSize(mediaBackend->videoSize());
    updateStatusFromBackend(*m_activePlaybackLifecycle);
    play();
}

void VideoDocumentRuntime::publishSourceLoadFailure(const VideoSourceLoadFailure& failure)
{
    invalidatePlaybackCallbacks();
    m_state.setEmbeddedMetadata({});
    m_state.setSourceLoadFailure(failure);
    updateZoomPercent();
}

void VideoDocumentRuntime::invalidatePlaybackCallbacks() { m_activePlaybackLifecycle.reset(); }

VideoDocumentRuntime::PlaybackLifecycle VideoDocumentRuntime::acceptPlaybackCallbacks(
    const QUrl& publicSourceUrl)
{
    ++m_nextPlaybackRevision;
    if (m_nextPlaybackRevision == 0) {
        ++m_nextPlaybackRevision;
    }
    PlaybackLifecycle lifecycle { m_nextPlaybackRevision, publicSourceUrl };
    m_activePlaybackLifecycle = lifecycle;
    return lifecycle;
}

bool VideoDocumentRuntime::playbackCallbacksAccepted(const PlaybackLifecycle& lifecycle) const
{
    return m_activePlaybackLifecycle.has_value()
        && m_activePlaybackLifecycle->revision == lifecycle.revision
        && m_activePlaybackLifecycle->publicSourceUrl == lifecycle.publicSourceUrl;
}

void VideoDocumentRuntime::updateStatusFromBackend(const PlaybackLifecycle& lifecycle)
{
    if (!playbackCallbacksAccepted(lifecycle)) {
        return;
    }

    const VideoDocumentStatusPlan plan = videoDocumentStatusPlan(VideoDocumentStatusSnapshot {
        m_state.sourceUrl().isEmpty(),
        m_sourceLoadRuntime.active(),
        m_mediaBackend != nullptr,
        m_mediaBackend != nullptr ? m_mediaBackend->mediaStatus() : VideoMediaStatus::Null,
    });
    m_state.setStatusAndError(
        plan.status, plan.status == VideoDocumentStatus::Error ? m_state.errorString() : QString());
    VideoPlaybackControlMediaSnapshot snapshot = m_playbackControls.mediaSnapshot();
    if (m_mediaBackend != nullptr) {
        snapshot.ready = m_state.status() == VideoDocumentStatus::Ready && m_state.hasVideo();
        snapshot.durationMsec = m_mediaBackend->duration();
        snapshot.positionMsec = m_mediaBackend->position();
        snapshot.playing = m_mediaBackend->playing();
        snapshot.seekable = m_mediaBackend->seekable();
        snapshot.muted = m_mediaBackend->muted();
    }
    snapshot.mediaEnded = plan.mediaEnded;
    if (plan.clearPlaying) {
        snapshot.playing = false;
    }
    m_playbackControls.acceptMediaSnapshot(snapshot);
    updateZoomPercent();
}

void VideoDocumentRuntime::refreshPlaybackControlsFromBackend(const PlaybackLifecycle& lifecycle)
{
    if (m_mediaBackend == nullptr || !playbackCallbacksAccepted(lifecycle)) {
        return;
    }

    VideoPlaybackControlMediaSnapshot snapshot = m_playbackControls.mediaSnapshot();
    snapshot.ready = m_state.status() == VideoDocumentStatus::Ready && m_state.hasVideo();
    snapshot.durationMsec = m_mediaBackend->duration();
    snapshot.positionMsec = m_mediaBackend->position();
    snapshot.playing = m_mediaBackend->playing();
    snapshot.seekable = m_mediaBackend->seekable();
    snapshot.muted = m_mediaBackend->muted();
    m_playbackControls.acceptMediaSnapshot(snapshot);
}

void VideoDocumentRuntime::replacePlaybackControlSource()
{
    m_playbackControls.replaceSource(++m_playbackControlSourceRevision);
}

void VideoDocumentRuntime::updateErrorFromBackend(
    const PlaybackLifecycle& lifecycle, VideoMediaError error)
{
    if (m_mediaBackend == nullptr || !playbackCallbacksAccepted(lifecycle)) {
        return;
    }

    qCDebug(kiriviewVideoLog).noquote()
        << "playback backend failure"
        << "source=" << lifecycle.publicSourceUrl
        << "category=" << videoMediaErrorCategoryName(error.category)
        << "code=" << error.rawErrorCode << "detail=" << error.diagnosticDetail;
    m_state.setBackendFailure(VideoBackendFailure {
        lifecycle.publicSourceUrl,
        VideoBackendFailureKind::Playback,
        error.category,
        error.rawErrorCode,
        imageErrorText(ImageErrorTextId::OpenVideo),
        std::move(error.diagnosticDetail),
        VideoBackendFailureSeverity::Error,
        false,
    });
    refreshPlaybackControlsFromBackend(lifecycle);
    updateZoomPercent();
}

void VideoDocumentRuntime::updateZoomPercent()
{
    if (m_state.status() != VideoDocumentStatus::Ready || !m_state.hasVideo()) {
        m_state.setZoomPercent(std::nullopt);
        return;
    }

    const std::optional<int> zoomPercent = m_outputRuntime.zoomPercent();
    if (!zoomPercent.has_value()) {
        m_state.setZoomPercent(std::nullopt);
        return;
    }

    m_state.setZoomPercent(zoomPercent.value());
}

void VideoDocumentRuntime::publish(VideoDocumentChange change) { m_state.publish(change); }
}
