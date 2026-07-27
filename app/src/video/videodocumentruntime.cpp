// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "video/videodocumentruntime.h"

#include "localization/imageerrortext.h"
#include "metadata/embeddedmetadata.h"
#include "video/videodocumentstatusplan.h"

#include <QDebug>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QObject>
#include <cstddef>
#include <memory>
#include <utility>
#include <variant>

Q_LOGGING_CATEGORY(kiriviewVideoLog, "org.hnjae.kiriview.video", QtWarningMsg)

namespace {
constexpr std::size_t playbackCommandsPerDrain = 64;
constexpr std::size_t maximumPendingPlaybackCommands = 1024;

std::shared_ptr<kiriview::VideoPlaybackUrlResolver> sharedPlaybackUrlResolver(
    std::unique_ptr<kiriview::VideoPlaybackUrlResolver> resolver)
{
    if (resolver == nullptr) {
        resolver = kiriview::createDefaultVideoPlaybackUrlResolver();
    }
    return std::shared_ptr<kiriview::VideoPlaybackUrlResolver>(std::move(resolver));
}

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

kiriview::VideoPlaybackStateDelta stateDeltaAfterBackendOperations(
    const kiriview::VideoPlaybackControlPlan& plan, bool mediaBackendAvailable)
{
    kiriview::VideoPlaybackStateDelta delta = plan.stateDelta;
    if (!mediaBackendAvailable) {
        return delta;
    }

    for (const kiriview::VideoPlaybackBackendOperation& operation : plan.backendOperations) {
        if (std::holds_alternative<kiriview::SetVideoPlaybackPositionOperation>(operation)) {
            delta.position.reset();
        }
        if (std::holds_alternative<kiriview::StopVideoPlaybackOperation>(operation)) {
            delta.playing.reset();
        }
    }
    return delta;
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
    , m_playbackUrlResolver(sharedPlaybackUrlResolver(std::move(playbackUrlResolver)))
    , m_outputRuntime(documentObject,
          VideoOutputRuntimeCallbacks {
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)](QObject* videoOutput) {
                  if (lifetime.expired()) {
                      return;
                  }
                  const std::shared_ptr<VideoMediaBackend> mediaBackend = m_mediaBackend;
                  if (mediaBackend != nullptr) {
                      mediaBackend->setVideoOutput(videoOutput);
                  }
              },
              [this, lifetime = std::weak_ptr<void>(m_callbackLifetime)](bool videoOutputChanged) {
                  if (!lifetime.expired()) {
                      updateOutputProjection(videoOutputChanged);
                  }
              },
          })
{
    if (!m_mediaBackendFactory) {
        m_mediaBackendFactory = []() { return createDefaultVideoMediaBackend(); };
    }
}

void VideoDocumentRuntime::installMediaBackendCallbacks(
    VideoMediaBackend* backend, const PlaybackLifecycle& lifecycle)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    backend->setCallbacks(VideoMediaBackendCallbacks {
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired()) {
                updateStatusFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle](VideoMediaError error) {
            if (!lifetime.expired()) {
                updateErrorFromBackend(lifecycle, std::move(error));
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired() && playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired() && playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired() && playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired() && playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired()) {
                updateHasVideoFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired()) {
                updateHasAudioFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired()) {
                updateVideoSizeFromBackend(lifecycle);
            }
        },
        [this, lifetime, lifecycle]() {
            if (!lifetime.expired() && playbackCallbacksAccepted(lifecycle)) {
                refreshPlaybackControlsFromBackend(lifecycle);
            }
        },
        {},
    });
}

VideoDocumentRuntime::~VideoDocumentRuntime()
{
    m_callbackLifetime.reset();
    m_sourceTransition.cancel();
    m_sourceTransitionPhase = SourceTransitionPhase::Idle;
    const std::shared_ptr<VideoPlaybackUrlResolver> resolver = std::move(m_playbackUrlResolver);
    if (resolver != nullptr) {
        resolver->cancel();
        resolver->cleanup();
    }
    retirePlaybackSource();
}

QUrl VideoDocumentRuntime::sourceUrl() const { return m_state.sourceUrl(); }

void VideoDocumentRuntime::setSourceUrl(const QUrl& sourceUrl)
{
    if (m_state.sourceUrl() == sourceUrl && m_playbackSourceDevice == nullptr
        && (sourceUrl.isEmpty() || m_sourceTransitionPhase != SourceTransitionPhase::Idle
            || m_mediaBackend != nullptr)) {
        return;
    }

    const std::optional<SourceTransition> transition = beginSourceTransition(sourceUrl,
        sourceUrl.isEmpty() ? SourceTransitionPhase::ApplyingTerminalResult
                            : SourceTransitionPhase::ResolvingPlaybackUrl);
    if (!transition.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    retirePlaybackSource();
    if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
        return;
    }

    replacePlaybackControlSource();
    if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
        return;
    }

    if (sourceUrl.isEmpty()) {
        m_state.resetForClearedSource();
        if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
            return;
        }
        finishSourceTransition(*transition);
        return;
    }

    m_state.resetForSourceLoad(sourceUrl);
    if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
        return;
    }
    resolvePlaybackUrl(*transition);
}

void VideoDocumentRuntime::setSourceDevice(
    const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice)
{
    if (sourceUrl.isEmpty() || sourceDevice.device == nullptr) {
        setSourceUrl(QUrl());
        return;
    }

    const std::optional<SourceTransition> transition
        = beginSourceTransition(sourceUrl, SourceTransitionPhase::ApplyingTerminalResult);
    if (!transition.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    retirePlaybackSource();
    if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
        return;
    }

    replacePlaybackControlSource();
    if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
        return;
    }

    m_state.resetForSourceLoad(sourceUrl);
    if (lifetime.expired() || !sourceTransitionAccepted(*transition)) {
        return;
    }
    applyPlaybackSourceDevice(*transition, std::move(sourceDevice));
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

void VideoDocumentRuntime::setPosition(qint64 position) { requestPlaybackControlSeek(position); }

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
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    if (m_playbackControls.mediaSnapshot().muted == muted) {
        return;
    }

    const std::shared_ptr<VideoMediaBackend> mediaBackend = m_mediaBackend;
    const std::optional<PlaybackLifecycle> lifecycle = m_activePlaybackLifecycle;
    const quint64 commandRevision = m_muteCommandAdmission.next();
    VideoPlaybackControlMediaSnapshot snapshot = m_playbackControls.mediaSnapshot();
    snapshot.muted = muted;
    m_playbackControls.acceptMediaSnapshot(snapshot);
    if (lifetime.expired() || !m_muteCommandAdmission.accepts(commandRevision)
        || mediaBackend == nullptr) {
        return;
    }
    if (!lifecycle.has_value() || m_mediaBackend != mediaBackend
        || !playbackCallbacksAccepted(*lifecycle)) {
        return;
    }
    mediaBackend->setMuted(muted);
}

QObject* VideoDocumentRuntime::videoOutput() const { return m_outputRuntime.videoOutput(); }

const EmbeddedMetadata& VideoDocumentRuntime::embeddedMetadata() const
{
    return m_state.embeddedMetadata();
}

void VideoDocumentRuntime::setVideoOutputAttachment(
    QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect)
{
    m_outputRuntime.setVideoOutputAttachment(videoOutput, contentRect, sourceRect);
}

void VideoDocumentRuntime::play() { submitPlaybackCommand(PlaybackCommandKind::Play); }

void VideoDocumentRuntime::pause() { submitPlaybackCommand(PlaybackCommandKind::Pause); }

void VideoDocumentRuntime::stop() { submitPlaybackCommand(PlaybackCommandKind::Stop); }

void VideoDocumentRuntime::togglePlayback() { submitPlaybackCommand(PlaybackCommandKind::Toggle); }

void VideoDocumentRuntime::toggleMuted() { setMuted(!muted()); }

void VideoDocumentRuntime::seekBy(qint64 deltaMilliseconds)
{
    const std::optional<VideoPlaybackSeekScope> scope = m_playbackControls.seekScope();
    if (scope.has_value()) {
        submitPlaybackCommand(PlaybackCommandKind::RelativeSeek, deltaMilliseconds, scope);
    }
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

void VideoDocumentRuntime::beginPlaybackScrub()
{
    const std::optional<VideoPlaybackSeekScope> scope = m_playbackControls.seekScope();
    if (scope.has_value()) {
        submitPlaybackCommand(PlaybackCommandKind::BeginScrub, 0, scope);
    }
}

void VideoDocumentRuntime::updatePlaybackScrub(qint64 positionMsec)
{
    const std::optional<VideoPlaybackSeekScope> scope = m_playbackControls.seekScope();
    if (scope.has_value()) {
        submitPlaybackCommand(PlaybackCommandKind::UpdateScrub, positionMsec, scope);
    }
}

void VideoDocumentRuntime::commitPlaybackScrub()
{
    const std::optional<VideoPlaybackSeekScope> scope = m_playbackControls.seekScope();
    if (scope.has_value()) {
        submitPlaybackCommand(PlaybackCommandKind::CommitScrub, 0, scope);
    }
}

void VideoDocumentRuntime::cancelPlaybackScrub()
{
    const std::optional<VideoPlaybackSeekScope> scope = m_playbackControls.seekScope();
    if (scope.has_value()) {
        submitPlaybackCommand(PlaybackCommandKind::CancelScrub, 0, scope);
    }
}

void VideoDocumentRuntime::requestPlaybackControlSeek(qint64 positionMsec)
{
    const std::optional<VideoPlaybackSeekScope> scope = m_playbackControls.seekScope();
    if (scope.has_value()) {
        submitPlaybackCommand(PlaybackCommandKind::AbsoluteSeek, positionMsec, scope);
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

void VideoDocumentRuntime::submitPlaybackCommand(
    PlaybackCommandKind kind, qint64 value, std::optional<VideoPlaybackSeekScope> seekScope)
{
    if (m_pendingPlaybackCommands.size() >= maximumPendingPlaybackCommands) {
        qCWarning(kiriviewVideoLog)
            << "Rejected playback command because the pending command limit was reached";
        return;
    }

    m_pendingPlaybackCommands.push_back(PlaybackCommandRequest {
        kind,
        value,
        m_playbackControlSourceRevision,
        m_activePlaybackLifecycle,
        m_mediaBackend.get(),
        seekScope,
    });
    if (!m_playbackCommandDispatchActive && !m_playbackCommandDrainScheduled) {
        drainPlaybackCommands();
    }
}

void VideoDocumentRuntime::drainPlaybackCommands()
{
    if (m_playbackCommandDispatchActive) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    m_playbackCommandDispatchActive = true;
    std::size_t processedCommandCount = 0;
    while (!m_pendingPlaybackCommands.empty() && processedCommandCount < playbackCommandsPerDrain) {
        PlaybackCommandRequest request = std::move(m_pendingPlaybackCommands.front());
        m_pendingPlaybackCommands.pop_front();
        ++processedCommandCount;
        if (playbackCommandRequestAccepted(request)) {
            executePlaybackCommandRequest(request);
        }
        if (lifetime.expired()) {
            return;
        }
    }
    m_playbackCommandDispatchActive = false;
    if (!m_pendingPlaybackCommands.empty()) {
        schedulePlaybackCommandDrain();
    }
}

void VideoDocumentRuntime::schedulePlaybackCommandDrain()
{
    if (m_playbackCommandDrainScheduled) {
        return;
    }

    QObject* owner = m_documentObject.data();
    if (owner == nullptr) {
        qCWarning(kiriviewVideoLog)
            << "Discarded pending playback commands because their owner no longer exists";
        m_pendingPlaybackCommands.clear();
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    m_playbackCommandDrainScheduled = true;
    const bool scheduled = QMetaObject::invokeMethod(
        owner,
        [this, lifetime]() {
            if (lifetime.expired()) {
                return;
            }
            m_playbackCommandDrainScheduled = false;
            drainPlaybackCommands();
        },
        Qt::QueuedConnection);
    if (!scheduled) {
        m_playbackCommandDrainScheduled = false;
        qCWarning(kiriviewVideoLog) << "Could not schedule pending playback commands";
        m_pendingPlaybackCommands.clear();
    }
}

bool VideoDocumentRuntime::playbackCommandRequestAccepted(
    const PlaybackCommandRequest& request) const
{
    if (request.sourceRevision != m_playbackControlSourceRevision
        || request.mediaBackend != m_mediaBackend.get()
        || request.lifecycle.has_value() != m_activePlaybackLifecycle.has_value()) {
        return false;
    }
    if (request.lifecycle.has_value() && !playbackCallbacksAccepted(*request.lifecycle)) {
        return false;
    }
    return !request.seekScope.has_value()
        || m_playbackControls.acceptsSeekScope(*request.seekScope);
}

void VideoDocumentRuntime::executePlaybackCommandRequest(const PlaybackCommandRequest& request)
{
    switch (request.kind) {
    case PlaybackCommandKind::Play:
        executePlaybackControlPlan(videoPlaybackPlayPlan(playbackControlSnapshot()));
        return;
    case PlaybackCommandKind::Pause:
        executePlaybackControlPlan(videoPlaybackPausePlan(playbackControlSnapshot()));
        return;
    case PlaybackCommandKind::Stop:
        executePlaybackControlPlan(videoPlaybackStopPlan(playbackControlSnapshot()));
        return;
    case PlaybackCommandKind::Toggle:
        executePlaybackControlPlan(videoPlaybackTogglePlan(playbackControlSnapshot()));
        return;
    case PlaybackCommandKind::BeginScrub:
        m_playbackControls.beginScrub();
        return;
    case PlaybackCommandKind::UpdateScrub:
        m_playbackControls.updateScrub(request.value);
        return;
    case PlaybackCommandKind::CancelScrub:
        m_playbackControls.cancelScrub();
        return;
    case PlaybackCommandKind::CommitScrub:
    case PlaybackCommandKind::AbsoluteSeek:
    case PlaybackCommandKind::RelativeSeek:
        executePlaybackSeekRequest(request);
        return;
    }
}

void VideoDocumentRuntime::executePlaybackSeekRequest(const PlaybackCommandRequest& request)
{
    if (!request.seekScope.has_value()) {
        return;
    }

    qint64 targetPosition = request.value;
    if (request.kind == PlaybackCommandKind::RelativeSeek) {
        const VideoPlaybackControlPlan relativePlan
            = videoPlaybackSeekByPlan(playbackControlSnapshot(), request.value);
        if (!relativePlan.stateDelta.position.has_value()) {
            return;
        }
        targetPosition = *relativePlan.stateDelta.position;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const std::shared_ptr<VideoMediaBackend> mediaBackend = m_mediaBackend;
    const std::optional<PlaybackLifecycle> lifecycle = m_activePlaybackLifecycle;
    std::optional<VideoPlaybackSeekIntent> intent;
    if (request.kind == PlaybackCommandKind::CommitScrub) {
        intent = m_playbackControls.commitScrub();
    } else {
        intent = m_playbackControls.requestSeek(targetPosition);
    }
    if (lifetime.expired() || !playbackCommandRequestAccepted(request) || !intent.has_value()
        || mediaBackend == nullptr || mediaBackend.get() != request.mediaBackend
        || !lifecycle.has_value() || m_mediaBackend != mediaBackend
        || !playbackCallbacksAccepted(*lifecycle)
        || !m_playbackControls.acceptsSeekIntent(*intent)) {
        return;
    }
    executePlaybackControlPlan(
        videoPlaybackSetPositionPlan(playbackControlSnapshot(), intent->positionMsec), intent);
}

void VideoDocumentRuntime::executePlaybackControlPlan(
    const VideoPlaybackControlPlan& plan, std::optional<VideoPlaybackSeekIntent> seekIntent)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const std::shared_ptr<VideoMediaBackend> mediaBackend = m_mediaBackend;
    const std::optional<PlaybackLifecycle> lifecycle = m_activePlaybackLifecycle;
    const auto executionAccepted = [this, &lifetime, &mediaBackend, &lifecycle, &seekIntent]() {
        if (lifetime.expired()
            || (seekIntent.has_value() && !m_playbackControls.acceptsSeekIntent(*seekIntent))) {
            return false;
        }
        return mediaBackend == nullptr
            || (lifecycle.has_value() && m_mediaBackend == mediaBackend
                && playbackCallbacksAccepted(*lifecycle));
    };

    for (const VideoPlaybackBackendOperation& operation : plan.backendOperations) {
        if (!executionAccepted()) {
            return;
        }
        executePlaybackBackendOperation(operation);
        if (!executionAccepted()) {
            return;
        }
    }

    if (!executionAccepted()) {
        return;
    }
    applyPlaybackStateDelta(stateDeltaAfterBackendOperations(plan, mediaBackend != nullptr));
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

std::optional<VideoDocumentRuntime::SourceTransition> VideoDocumentRuntime::beginSourceTransition(
    const QUrl& sourceUrl, SourceTransitionPhase phase)
{
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    m_pendingPlaybackCommands.clear();
    SourceTransition transition = m_sourceTransition.start(sourceUrl);
    m_sourceTransitionPhase = phase;
    invalidatePlaybackCallbacks();

    const std::shared_ptr<VideoPlaybackUrlResolver> resolver = m_playbackUrlResolver;
    if (resolver == nullptr) {
        return transition;
    }

    resolver->cancel();
    if (lifetime.expired() || !sourceTransitionAccepted(transition)) {
        return std::nullopt;
    }
    resolver->cleanup();
    if (lifetime.expired() || !sourceTransitionAccepted(transition)) {
        return std::nullopt;
    }
    return transition;
}

bool VideoDocumentRuntime::sourceTransitionAccepted(const SourceTransition& transition) const
{
    return m_sourceTransition.accepts(transition);
}

bool VideoDocumentRuntime::sourceBackendAccepted(const SourceTransition& transition,
    const PlaybackLifecycle& lifecycle, const VideoMediaBackend* backend) const
{
    return sourceTransitionAccepted(transition) && playbackCallbacksAccepted(lifecycle)
        && m_mediaBackend.get() == backend;
}

void VideoDocumentRuntime::resolvePlaybackUrl(const SourceTransition& transition)
{
    const std::shared_ptr<VideoPlaybackUrlResolver> resolver = m_playbackUrlResolver;
    if (resolver == nullptr || !sourceTransitionAccepted(transition)
        || m_sourceTransitionPhase != SourceTransitionPhase::ResolvingPlaybackUrl) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    resolver->resolve(
        transition.operationId, transition.scope, m_documentObject.data(),
        [this, lifetime, transition](const VideoPlaybackUrlResolution& resolution) {
            if (!lifetime.expired()) {
                completePlaybackUrlResolution(transition, resolution);
            }
        },
        [this, lifetime, transition](
            quint64 operationId, const QUrl& sourceUrl, const QString& diagnosticDetail) {
            if (!lifetime.expired()) {
                failPlaybackUrlResolution(transition, operationId, sourceUrl, diagnosticDetail);
            }
        });
}

void VideoDocumentRuntime::completePlaybackUrlResolution(
    const SourceTransition& transition, const VideoPlaybackUrlResolution& resolution)
{
    if (resolution.operationId != transition.operationId || resolution.sourceUrl != transition.scope
        || !sourceTransitionAccepted(transition)
        || m_sourceTransitionPhase != SourceTransitionPhase::ResolvingPlaybackUrl) {
        return;
    }

    m_sourceTransitionPhase = SourceTransitionPhase::ApplyingTerminalResult;
    applyResolvedPlaybackUrl(transition, resolution.playbackUrl);
}

void VideoDocumentRuntime::failPlaybackUrlResolution(const SourceTransition& transition,
    quint64 operationId, const QUrl& sourceUrl, const QString& diagnosticDetail)
{
    if (operationId != transition.operationId || sourceUrl != transition.scope
        || !sourceTransitionAccepted(transition)
        || m_sourceTransitionPhase != SourceTransitionPhase::ResolvingPlaybackUrl) {
        return;
    }

    m_sourceTransitionPhase = SourceTransitionPhase::ApplyingTerminalResult;
    publishSourceLoadFailure(transition,
        makeSourceLoadFailure(
            transition, VideoSourceLoadFailureKind::PlaybackUrlResolution, diagnosticDetail));
}

void VideoDocumentRuntime::finishSourceTransition(const SourceTransition& transition)
{
    if (m_sourceTransition.finish(transition)) {
        m_sourceTransitionPhase = SourceTransitionPhase::Idle;
    }
}

void VideoDocumentRuntime::retirePlaybackSource()
{
    invalidatePlaybackCallbacks();
    const std::shared_ptr<VideoPlaybackSourceDevice> retiredSourceDevice
        = std::move(m_playbackSourceDevice);
    const std::shared_ptr<VideoMediaBackend> retiredMediaBackend = std::move(m_mediaBackend);
    if (retiredMediaBackend == nullptr) {
        return;
    }

    retiredMediaBackend->stop();
    retiredMediaBackend->setCallbacks({});
    retiredMediaBackend->setSource(QUrl());
    retiredMediaBackend->setVideoOutput(nullptr);
}

void VideoDocumentRuntime::applyResolvedPlaybackUrl(
    const SourceTransition& transition, const QUrl& playbackUrl)
{
    const EmbeddedMetadata metadata = playbackUrl.isLocalFile()
        ? parsePathEmbeddedMetadata(playbackUrl.toLocalFile())
        : EmbeddedMetadata {};
    const MediaBackendFactory mediaBackendFactory = m_mediaBackendFactory;
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    std::unique_ptr<VideoMediaBackend> candidate
        = mediaBackendFactory ? mediaBackendFactory() : nullptr;
    if (lifetime.expired() || !sourceTransitionAccepted(transition)) {
        return;
    }
    if (candidate == nullptr) {
        publishSourceLoadFailure(transition,
            makeSourceLoadFailure(transition, VideoSourceLoadFailureKind::PlaybackBackendCreation,
                QStringLiteral("Could not create the video playback backend.")));
        return;
    }

    const std::shared_ptr<VideoMediaBackend> mediaBackend(std::move(candidate));
    m_mediaBackend = mediaBackend;
    m_playbackSourceDevice.reset();
    const PlaybackLifecycle lifecycle = acceptPlaybackCallbacks(transition.scope);
    installMediaBackendCallbacks(mediaBackend.get(), lifecycle);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    mediaBackend->setMuted(m_playbackControls.mediaSnapshot().muted);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    m_outputRuntime.backendVideoOutputTargetChanged();
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    mediaBackend->setSource(playbackUrl);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    m_state.setEmbeddedMetadata(metadata);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    updateVideoSizeFromBackend(lifecycle);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    updateStatusFromBackend(lifecycle);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    play();
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    finishSourceTransition(transition);
}

void VideoDocumentRuntime::applyPlaybackSourceDevice(
    const SourceTransition& transition, VideoPlaybackSourceDevice sourceDevice)
{
    const std::shared_ptr<VideoPlaybackSourceDevice> sourceDeviceLease
        = std::make_shared<VideoPlaybackSourceDevice>(std::move(sourceDevice));
    const MediaBackendFactory mediaBackendFactory = m_mediaBackendFactory;
    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    std::unique_ptr<VideoMediaBackend> candidate
        = mediaBackendFactory ? mediaBackendFactory() : nullptr;
    if (lifetime.expired() || !sourceTransitionAccepted(transition)) {
        return;
    }
    if (candidate == nullptr) {
        publishSourceLoadFailure(transition,
            makeSourceLoadFailure(transition, VideoSourceLoadFailureKind::PlaybackBackendCreation,
                QStringLiteral("Could not create the video playback backend.")));
        return;
    }

    const std::shared_ptr<VideoMediaBackend> mediaBackend(std::move(candidate));
    m_playbackSourceDevice = sourceDeviceLease;
    m_mediaBackend = mediaBackend;
    const PlaybackLifecycle lifecycle = acceptPlaybackCallbacks(transition.scope);
    installMediaBackendCallbacks(mediaBackend.get(), lifecycle);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    mediaBackend->setMuted(m_playbackControls.mediaSnapshot().muted);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    m_outputRuntime.backendVideoOutputTargetChanged();
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    mediaBackend->setSourceDevice(sourceDeviceLease->device.get(), transition.scope);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }

    updateVideoSizeFromBackend(lifecycle);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    updateStatusFromBackend(lifecycle);
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    play();
    if (lifetime.expired() || !sourceBackendAccepted(transition, lifecycle, mediaBackend.get())) {
        return;
    }
    finishSourceTransition(transition);
}

void VideoDocumentRuntime::publishSourceLoadFailure(
    const SourceTransition& transition, VideoSourceLoadFailure failure)
{
    if (!sourceTransitionAccepted(transition)) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    invalidatePlaybackCallbacks();
    m_state.setSourceLoadFailure(std::move(failure));
    if (lifetime.expired() || !sourceTransitionAccepted(transition)) {
        return;
    }
    finishSourceTransition(transition);
}

VideoSourceLoadFailure VideoDocumentRuntime::makeSourceLoadFailure(
    const SourceTransition& transition, VideoSourceLoadFailureKind kind,
    QString diagnosticDetail) const
{
    return VideoSourceLoadFailure {
        transition.scope,
        kind,
        imageErrorText(ImageErrorTextId::OpenVideo),
        std::move(diagnosticDetail),
        VideoSourceLoadFailureSeverity::Error,
        false,
    };
}

void VideoDocumentRuntime::invalidatePlaybackCallbacks()
{
    m_muteCommandAdmission.invalidate();
    m_backendObservationAdmission.invalidate();
    m_activePlaybackLifecycle.reset();
}

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

std::optional<VideoDocumentRuntime::BackendObservation>
VideoDocumentRuntime::beginBackendObservation(const PlaybackLifecycle& lifecycle)
{
    std::shared_ptr<VideoMediaBackend> mediaBackend = m_mediaBackend;
    if (mediaBackend == nullptr || !playbackCallbacksAccepted(lifecycle)) {
        return std::nullopt;
    }

    return BackendObservation {
        m_backendObservationAdmission.next(),
        lifecycle,
        std::move(mediaBackend),
    };
}

bool VideoDocumentRuntime::backendObservationAccepted(const BackendObservation& observation) const
{
    return m_backendObservationAdmission.accepts(observation.revision)
        && m_mediaBackend == observation.backend
        && playbackCallbacksAccepted(observation.lifecycle);
}

void VideoDocumentRuntime::updateHasVideoFromBackend(const PlaybackLifecycle& lifecycle)
{
    const std::optional<BackendObservation> observation = beginBackendObservation(lifecycle);
    if (!observation.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const bool hasVideo = observation->backend->hasVideo();
    if (lifetime.expired() || !backendObservationAccepted(*observation)) {
        return;
    }

    m_state.setHasVideo(hasVideo);
    if (lifetime.expired() || m_mediaBackend != observation->backend
        || !playbackCallbacksAccepted(observation->lifecycle)) {
        return;
    }

    refreshPlaybackControlsFromBackend(observation->lifecycle);
    if (lifetime.expired() || m_mediaBackend != observation->backend
        || !playbackCallbacksAccepted(observation->lifecycle)) {
        return;
    }
    updateZoomPercent();
}

void VideoDocumentRuntime::updateHasAudioFromBackend(const PlaybackLifecycle& lifecycle)
{
    const std::optional<BackendObservation> observation = beginBackendObservation(lifecycle);
    if (!observation.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const bool hasAudio = observation->backend->hasAudio();
    if (lifetime.expired() || !backendObservationAccepted(*observation)) {
        return;
    }

    m_state.setHasAudio(hasAudio);
}

void VideoDocumentRuntime::updateVideoSizeFromBackend(const PlaybackLifecycle& lifecycle)
{
    const std::optional<BackendObservation> observation = beginBackendObservation(lifecycle);
    if (!observation.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const QSize videoSize = observation->backend->videoSize();
    if (lifetime.expired() || !backendObservationAccepted(*observation)) {
        return;
    }

    m_state.setVideoSize(videoSize);
}

void VideoDocumentRuntime::updateStatusFromBackend(const PlaybackLifecycle& lifecycle)
{
    const std::optional<BackendObservation> observation = beginBackendObservation(lifecycle);
    if (!observation.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const VideoMediaStatus mediaStatus = observation->backend->mediaStatus();
    if (lifetime.expired() || !backendObservationAccepted(*observation)) {
        return;
    }

    const VideoDocumentStatusPlan plan = videoDocumentStatusPlan(VideoDocumentStatusSnapshot {
        m_state.sourceUrl().isEmpty(),
        m_sourceTransitionPhase == SourceTransitionPhase::ResolvingPlaybackUrl,
        true,
        mediaStatus,
    });
    m_state.setStatusAndError(
        plan.status, plan.status == VideoDocumentStatus::Error ? m_state.errorString() : QString());
    if (lifetime.expired() || m_mediaBackend != observation->backend
        || !playbackCallbacksAccepted(observation->lifecycle)) {
        return;
    }

    refreshPlaybackControlsFromBackend(observation->lifecycle, true);
    if (lifetime.expired() || m_mediaBackend != observation->backend
        || !playbackCallbacksAccepted(observation->lifecycle)) {
        return;
    }

    updateZoomPercent();
}

void VideoDocumentRuntime::refreshPlaybackControlsFromBackend(
    const PlaybackLifecycle& lifecycle, bool reconcileMediaStatus)
{
    const std::optional<BackendObservation> observation = beginBackendObservation(lifecycle);
    if (!observation.has_value()) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
    const VideoMediaStatus mediaStatus
        = reconcileMediaStatus ? observation->backend->mediaStatus() : VideoMediaStatus::Null;
    const qint64 duration = observation->backend->duration();
    const qint64 position = observation->backend->position();
    const bool playing = observation->backend->playing();
    const bool seekable = observation->backend->seekable();
    const bool muted = observation->backend->muted();
    if (lifetime.expired() || !backendObservationAccepted(*observation)) {
        return;
    }

    std::optional<VideoDocumentStatusPlan> statusPlan;
    if (reconcileMediaStatus) {
        statusPlan = videoDocumentStatusPlan(VideoDocumentStatusSnapshot {
            m_state.sourceUrl().isEmpty(),
            m_sourceTransitionPhase == SourceTransitionPhase::ResolvingPlaybackUrl,
            true,
            mediaStatus,
        });
    }
    VideoPlaybackControlMediaSnapshot snapshot = m_playbackControls.mediaSnapshot();
    snapshot.ready = m_state.status() == VideoDocumentStatus::Ready && m_state.hasVideo();
    snapshot.durationMsec = duration;
    snapshot.positionMsec = position;
    snapshot.playing = playing;
    snapshot.seekable = seekable;
    snapshot.muted = muted;
    if (statusPlan.has_value()) {
        snapshot.mediaEnded = statusPlan->mediaEnded;
        if (statusPlan->clearPlaying) {
            snapshot.playing = false;
        }
    }
    m_playbackControls.acceptMediaSnapshot(snapshot);
}

void VideoDocumentRuntime::replacePlaybackControlSource()
{
    m_playbackControls.replaceSource(++m_playbackControlSourceRevision);
}

void VideoDocumentRuntime::updateErrorFromBackend(
    const PlaybackLifecycle& lifecycle, VideoMediaError error)
{
    const std::shared_ptr<VideoMediaBackend> mediaBackend = m_mediaBackend;
    if (mediaBackend == nullptr || !playbackCallbacksAccepted(lifecycle)) {
        return;
    }

    const std::weak_ptr<void> lifetime = m_callbackLifetime;
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
    if (lifetime.expired() || m_mediaBackend != mediaBackend
        || !playbackCallbacksAccepted(lifecycle)) {
        return;
    }
    refreshPlaybackControlsFromBackend(lifecycle);
    if (lifetime.expired() || m_mediaBackend != mediaBackend
        || !playbackCallbacksAccepted(lifecycle)) {
        return;
    }
    updateZoomPercent();
}

void VideoDocumentRuntime::updateZoomPercent() { updateOutputProjection(false); }

void VideoDocumentRuntime::updateOutputProjection(bool videoOutputChanged)
{
    if (m_state.status() != VideoDocumentStatus::Ready || !m_state.hasVideo()) {
        m_state.applyVideoOutputProjection(std::nullopt, videoOutputChanged);
        return;
    }

    const std::optional<int> zoomPercent = m_outputRuntime.zoomPercent();
    m_state.applyVideoOutputProjection(zoomPercent, videoOutputChanged);
}

void VideoDocumentRuntime::publish(VideoDocumentChange change) { m_state.publish(change); }
}
