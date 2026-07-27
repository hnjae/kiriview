// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEODOCUMENTRUNTIME_H
#define KIRIVIEW_VIDEODOCUMENTRUNTIME_H

#include "async/imageasyncoperationstate.h"
#include "async/imageasyncticket.h"
#include "metadata/embeddedmetadata.h"
#include "video/videodocumentstate.h"
#include "video/videomediabackend.h"
#include "video/videooutputruntime.h"
#include "video/videoplaybackcontrolplan.h"
#include "video/videoplaybackcontrolruntime.h"
#include "video/videoplaybacksource.h"
#include "video/videoplaybackurlresolver.h"

#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QUrl>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace kiriview {
class VideoDocumentRuntime final
{
public:
    using ChangeCallback = std::function<void(const std::vector<VideoDocumentChange>&)>;
    using MediaBackendFactory = VideoMediaBackendFactory;

    explicit VideoDocumentRuntime(QObject* documentObject, ChangeCallback changeCallback = {},
        std::unique_ptr<VideoPlaybackUrlResolver> playbackUrlResolver = {},
        MediaBackendFactory mediaBackendFactory = {},
        TimerScheduler playbackControlTimerScheduler = {},
        VideoPlaybackControlProjectionCallback playbackControlProjectionCallback = {});
    ~VideoDocumentRuntime();
    Q_DISABLE_COPY_MOVE(VideoDocumentRuntime)

    QUrl sourceUrl() const;
    void setSourceUrl(const QUrl& sourceUrl);
    void setSourceDevice(const QUrl& sourceUrl, VideoPlaybackSourceDevice sourceDevice);
    VideoDocumentStatus status() const;
    QString errorString() const;
    const std::optional<VideoSourceLoadFailure>& sourceLoadFailure() const;
    const std::optional<VideoBackendFailure>& backendFailure() const;
    QString windowTitleFileName() const;
    qint64 duration() const;
    qint64 position() const;
    void setPosition(qint64 position);
    bool playing() const;
    bool seekable() const;
    bool hasVideo() const;
    bool hasAudio() const;
    QSize videoSize() const;
    bool zoomPercentKnown() const;
    int zoomPercent() const;
    bool muted() const;
    void setMuted(bool muted);
    QObject* videoOutput() const;
    const EmbeddedMetadata& embeddedMetadata() const;
    void setVideoOutputAttachment(
        QObject* videoOutput, const QRectF& contentRect, const QRectF& sourceRect);

    void play();
    void pause();
    void stop();
    void togglePlayback();
    void toggleMuted();
    void seekBy(qint64 deltaMilliseconds);
    const VideoPlaybackControlProjection& playbackControlProjection() const;
    void reportPlaybackControlEnvironment(VideoPlaybackControlEnvironment environment);
    void reportPlaybackControlInteraction(bool active);
    void revealPlaybackControls();
    void beginPlaybackScrub();
    void updatePlaybackScrub(qint64 positionMsec);
    void commitPlaybackScrub();
    void cancelPlaybackScrub();
    void requestPlaybackControlSeek(qint64 positionMsec);

private:
    enum class SourceTransitionPhase {
        Idle,
        ResolvingPlaybackUrl,
        ApplyingTerminalResult,
    };

    using SourceTransition = ImageAsyncScopedOperation<QUrl>;

    struct PlaybackLifecycle
    {
        quint64 revision = 0;
        QUrl publicSourceUrl;
    };

    struct BackendObservation
    {
        quint64 revision = 0;
        PlaybackLifecycle lifecycle;
        std::shared_ptr<VideoMediaBackend> backend;
    };

    enum class PlaybackCommandKind {
        Play,
        Pause,
        Stop,
        Toggle,
        BeginScrub,
        UpdateScrub,
        CommitScrub,
        CancelScrub,
        AbsoluteSeek,
        RelativeSeek,
    };

    struct PlaybackCommandRequest
    {
        PlaybackCommandKind kind = PlaybackCommandKind::Play;
        qint64 value = 0;
        quint64 sourceRevision = 0;
        std::optional<PlaybackLifecycle> lifecycle;
        VideoMediaBackend* mediaBackend = nullptr;
        std::optional<VideoPlaybackSeekScope> seekScope;
    };

    VideoPlaybackControlSnapshot playbackControlSnapshot() const;
    void submitPlaybackCommand(PlaybackCommandKind kind, qint64 value = 0,
        std::optional<VideoPlaybackSeekScope> seekScope = std::nullopt);
    void drainPlaybackCommands();
    void schedulePlaybackCommandDrain();
    [[nodiscard]] bool playbackCommandRequestAccepted(const PlaybackCommandRequest& request) const;
    void executePlaybackCommandRequest(const PlaybackCommandRequest& request);
    void executePlaybackSeekRequest(const PlaybackCommandRequest& request);
    void executePlaybackControlPlan(const VideoPlaybackControlPlan& plan,
        std::optional<VideoPlaybackSeekIntent> seekIntent = std::nullopt);
    void executePlaybackBackendOperation(VideoPlaybackBackendOperation operation);
    void executePlaybackBackendOperation(EnsureVideoPlaybackBackendOperation operation);
    void executePlaybackBackendOperation(PlayVideoPlaybackOperation operation);
    void executePlaybackBackendOperation(PauseVideoPlaybackOperation operation);
    void executePlaybackBackendOperation(StopVideoPlaybackOperation operation);
    void executePlaybackBackendOperation(SetVideoPlaybackPositionOperation operation);
    void applyPlaybackStateDelta(const VideoPlaybackStateDelta& delta);
    std::optional<SourceTransition> beginSourceTransition(
        const QUrl& sourceUrl, SourceTransitionPhase phase);
    [[nodiscard]] bool sourceTransitionAccepted(const SourceTransition& transition) const;
    [[nodiscard]] bool sourceBackendAccepted(const SourceTransition& transition,
        const PlaybackLifecycle& lifecycle, const VideoMediaBackend* backend) const;
    void resolvePlaybackUrl(const SourceTransition& transition);
    void completePlaybackUrlResolution(
        const SourceTransition& transition, const VideoPlaybackUrlResolution& resolution);
    void failPlaybackUrlResolution(const SourceTransition& transition, quint64 operationId,
        const QUrl& sourceUrl, const QString& diagnosticDetail);
    void finishSourceTransition(const SourceTransition& transition);
    void retirePlaybackSource();
    void installMediaBackendCallbacks(
        VideoMediaBackend* backend, const PlaybackLifecycle& lifecycle);
    void applyResolvedPlaybackUrl(const SourceTransition& transition, const QUrl& playbackUrl);
    void applyPlaybackSourceDevice(
        const SourceTransition& transition, VideoPlaybackSourceDevice sourceDevice);
    void publishSourceLoadFailure(
        const SourceTransition& transition, VideoSourceLoadFailure failure);
    VideoSourceLoadFailure makeSourceLoadFailure(const SourceTransition& transition,
        VideoSourceLoadFailureKind kind, QString diagnosticDetail) const;
    void invalidatePlaybackCallbacks();
    PlaybackLifecycle acceptPlaybackCallbacks(const QUrl& publicSourceUrl);
    bool playbackCallbacksAccepted(const PlaybackLifecycle& lifecycle) const;
    std::optional<BackendObservation> beginBackendObservation(const PlaybackLifecycle& lifecycle);
    bool backendObservationAccepted(const BackendObservation& observation) const;
    void updateHasVideoFromBackend(const PlaybackLifecycle& lifecycle);
    void updateHasAudioFromBackend(const PlaybackLifecycle& lifecycle);
    void updateVideoSizeFromBackend(const PlaybackLifecycle& lifecycle);
    void updateStatusFromBackend(const PlaybackLifecycle& lifecycle);
    void updateErrorFromBackend(const PlaybackLifecycle& lifecycle, VideoMediaError error);
    void refreshPlaybackControlsFromBackend(
        const PlaybackLifecycle& lifecycle, bool reconcileMediaStatus = false);
    void replacePlaybackControlSource();
    void updateOutputProjection(bool videoOutputChanged);
    void updateZoomPercent();
    void publish(VideoDocumentChange change);

    QPointer<QObject> m_documentObject;
    std::shared_ptr<void> m_callbackLifetime = std::make_shared<char>();
    VideoDocumentState m_state;
    VideoPlaybackControlRuntime m_playbackControls;
    std::shared_ptr<VideoMediaBackend> m_mediaBackend;
    MediaBackendFactory m_mediaBackendFactory;
    std::shared_ptr<VideoPlaybackUrlResolver> m_playbackUrlResolver;
    ImageAsyncScopedOperationState<QUrl> m_sourceTransition;
    SourceTransitionPhase m_sourceTransitionPhase = SourceTransitionPhase::Idle;
    std::shared_ptr<VideoPlaybackSourceDevice> m_playbackSourceDevice;
    VideoOutputRuntime m_outputRuntime;
    quint64 m_nextPlaybackRevision = 0;
    std::optional<PlaybackLifecycle> m_activePlaybackLifecycle;
    ImageAsyncTicket m_backendObservationAdmission;
    quint64 m_playbackControlSourceRevision = 0;
    ImageAsyncTicket m_muteCommandAdmission;
    std::deque<PlaybackCommandRequest> m_pendingPlaybackCommands;
    bool m_playbackCommandDispatchActive = false;
    bool m_playbackCommandDrainScheduled = false;
};
}

#endif
