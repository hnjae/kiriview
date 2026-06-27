#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <limits>

using namespace ImageViewportInternal;

namespace {
struct PlaybackAdvanceTarget {
    int frame = -1;
    int requestedPosition = -1;
    int playbackPosition = -1;
    bool reachedEnd = false;
    bool looped = false;
    bool valid = false;
};

template <typename FrameStartFor, typename FrameIndexFor>
PlaybackAdvanceTarget playbackAdvanceTarget(int elapsedMilliseconds, int currentFrame,
    int currentPlaybackPosition, bool looping, int totalDuration, int frameCount,
    FrameStartFor frameStartFor, FrameIndexFor frameIndexFor)
{
    PlaybackAdvanceTarget target;
    int nextPlaybackPosition
        = currentPlaybackPosition < 0 ? frameStartFor(currentFrame) : currentPlaybackPosition;
    nextPlaybackPosition += elapsedMilliseconds;

    if (nextPlaybackPosition >= totalDuration) {
        if (looping) {
            const int wrappedPosition = totalDuration > 0 ? nextPlaybackPosition % totalDuration : 0;
            const int wrappedFrame = frameIndexFor(wrappedPosition);
            if (wrappedFrame < 0) {
                return target;
            }
            target.frame = wrappedFrame;
            target.playbackPosition = wrappedPosition;
            target.requestedPosition = frameStartFor(wrappedFrame);
            target.looped = true;
            target.valid = true;
            return target;
        }

        const int finalFrame = frameCount - 1;
        target.frame = finalFrame;
        target.requestedPosition = frameStartFor(finalFrame);
        target.playbackPosition = totalDuration;
        target.reachedEnd = true;
        target.valid = true;
        return target;
    }

    const int nextFrame = frameIndexFor(nextPlaybackPosition);
    if (nextFrame < 0) {
        return target;
    }
    target.frame = nextFrame;
    target.requestedPosition = frameStartFor(nextFrame);
    target.playbackPosition = nextPlaybackPosition;
    target.valid = true;
    return target;
}

void applyPlaybackTarget(ImageViewportPrivate& viewport, int frame, int requestedPosition)
{
    viewport.beginDisplayRequest(DisplayRequestOrigin::Playback, false);
    viewport.m_currentFrame = frame;
    viewport.m_requestedPosition = requestedPosition;
}

void publishPlaybackRequestChange(ImageViewportPrivate& viewport, int previousFrame)
{
    viewport.incrementRequestRevision();
    if (viewport.m_currentFrame != previousFrame
        || viewport.m_displayStatus != ImageViewport::DisplayStatus::Ready) {
        viewport.incrementDisplayRevision();
    }
    emit viewport.q->requestStateChanged();
    emit viewport.q->displayStateChanged();
}
}

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    if (m_playbackPhase != PlaybackPhase::Playing || elapsedMilliseconds <= 0) {
        return;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        const int duration = totalDuration();
        const int previousFrame = m_currentFrame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, m_currentFrame, m_playbackPosition, m_looping, duration,
            frameCount(), [this](int frame) { return providerFrameStartPosition(frame); },
            [this](int position) { return providerFrameIndexForPosition(position); });
        if (!target.valid) {
            return;
        }
        if (target.reachedEnd) {
            m_stopPlaybackWhenRequestReady = true;
        }

        m_playbackPosition = target.playbackPosition;
        if (target.frame == previousFrame && m_requestStatus == RequestStatus::Ready) {
            if (m_stopPlaybackWhenRequestReady) {
                setPlaybackPhase(PlaybackPhase::Stopped);
                m_stopPlaybackWhenRequestReady = false;
            }
            return;
        }

        applyPlaybackTarget(*this, target.frame, target.requestedPosition);
        m_currentProviderTargetKind = ProviderRequestTargetKind::Playback;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus
            = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        discardPendingRenderCommit();
        const bool diagnosticsValueChanged = clearDiagnostics();
        if (m_activeProviderFrameToken.isValid()) {
            queueProviderFrameRequest(target.frame, ProviderRequestTargetKind::Playback);
        } else if (!startProviderFrameRequest(target.frame, ProviderRequestTargetKind::Playback)) {
            publishPlaybackRequestChange(*this, previousFrame);
            emit q->diagnosticsChanged();
            update();
            return;
        }
        setPlaybackPhase(PlaybackPhase::Waiting);
        publishPlaybackRequestChange(*this, previousFrame);
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        update();
        return;
    }

    if (!hasTimedSequence()) {
        return;
    }

    const int totalDuration = m_sequence->totalDuration();
    const int previousFrame = m_currentFrame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, m_currentFrame, m_playbackPosition, m_looping, totalDuration,
        m_sequence->frameCount(), [this](int frame) { return m_sequence->frameStartPosition(frame); },
        [this](int position) { return m_sequence->frameIndexForPosition(position); });
    if (!target.valid) {
        return;
    }

    m_playbackPosition = target.playbackPosition;
    if (!target.reachedEnd && !target.looped && target.frame == m_currentFrame) {
        return;
    }

    applyPlaybackTarget(*this, target.frame, target.requestedPosition);
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    publishAcceptedTargetState();
    if (target.reachedEnd) {
        m_stopPlaybackWhenRequestReady = m_requestStatus == RequestStatus::Loading;
        setPlaybackPhase(
            m_stopPlaybackWhenRequestReady ? PlaybackPhase::Waiting : PlaybackPhase::Stopped);
    } else {
        setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting
                                                                   : PlaybackPhase::Playing);
    }
    publishPlaybackRequestChange(*this, previousFrame);
    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    update();
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTestImpl(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
    syncPlaybackTimer();
}

void ImageViewportPrivate::setNextProviderRequestTokenForTestImpl(quint64 token)
{
    m_nextProviderRequestToken = token;
}

bool ImageViewportPrivate::hasPendingRenderCommitForTestImpl() const
{
    return m_renderCommitPending;
}

quint64 ImageViewportPrivate::activeRequestIdForTestImpl() const { return m_activeRequestId; }

quint64 ImageViewportPrivate::displayedRequestIdForTestImpl() const
{
    return m_displayedRequestId;
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTestImpl() const
{
    return m_pendingPreparedPayloadId;
}
#endif

void ImageViewportPrivate::incrementDisplayRevision()
{
    ++m_displayRevision;
    emit q->displayRevisionChanged();
}

void ImageViewportPrivate::incrementRequestRevision()
{
    ++m_requestRevision;
    emit q->requestRevisionChanged();
}

void ImageViewportPrivate::setPlaybackPhase(PlaybackPhase phase)
{
    if (m_playbackPhase == phase) {
        return;
    }

    m_playbackPhase = phase;
    emit q->playbackPhaseChanged();
    syncPlaybackTimer();
}

void ImageViewportPrivate::syncPlaybackTimer()
{
    const int interval = playbackTimerInterval();
    if (interval <= 0) {
        stopPlaybackTimer();
        return;
    }

    playbackElapsedTimer.restart();
    playbackTimer.start(interval);
}

void ImageViewportPrivate::stopPlaybackTimer()
{
    playbackTimer.stop();
    playbackElapsedTimer.invalidate();
}

void ImageViewportPrivate::handlePlaybackTimer()
{
    advancePlayback(takePlaybackTimerElapsed());
    syncPlaybackTimer();
}

int ImageViewportPrivate::takePlaybackTimerElapsed()
{
    const qint64 elapsedMilliseconds
        = playbackElapsedTimer.isValid() ? playbackElapsedTimer.elapsed() : 0;
    playbackTimer.stop();
    playbackElapsedTimer.invalidate();
    return static_cast<int>(std::min<qint64>(elapsedMilliseconds, std::numeric_limits<int>::max()));
}

void ImageViewportPrivate::flushPlaybackTimerElapsed()
{
    if (!playbackElapsedTimer.isValid()) {
        return;
    }

    advancePlayback(takePlaybackTimerElapsed());
}

int ImageViewportPrivate::playbackTimerInterval() const
{
    if (m_playbackPhase != PlaybackPhase::Playing || m_requestStatus != RequestStatus::Ready) {
        return -1;
    }

    int frameStart = -1;
    int frameDuration = -1;
    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        if (m_currentFrame < 0 || m_currentFrame >= m_providerTimingIntervals.frameCount()) {
            return -1;
        }
        frameStart = providerFrameStartPosition(m_currentFrame);
        frameDuration = m_providerTimingIntervals.frameDuration(m_currentFrame);
    } else if (hasTimedSequence()) {
        if (m_currentFrame < 0 || m_currentFrame >= m_sequence->frameCount()) {
            return -1;
        }
        frameStart = m_sequence->frameStartPosition(m_currentFrame);
        const int nextFrameStart = m_currentFrame + 1 < m_sequence->frameCount()
            ? m_sequence->frameStartPosition(m_currentFrame + 1)
            : m_sequence->totalDuration();
        frameDuration = nextFrameStart - frameStart;
    } else {
        return -1;
    }

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition = m_playbackPosition >= 0 ? m_playbackPosition : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

void ImageViewportPrivate::setCommandDiagnostic(CommandReason reason)
{
    m_commandReason = reason;
    ++m_commandRevision;
    emit q->commandRevisionChanged();
    emit q->commandStateChanged();
}

void ImageViewportPrivate::clearCommandDiagnosticForAcceptedCommand()
{
    if (m_commandReason == CommandReason::NoCommand) {
        return;
    }

    setCommandDiagnostic(CommandReason::NoCommand);
}

bool ImageViewportPrivate::clearDiagnostics()
{
    if (m_errorString.isEmpty() && m_warningString.isEmpty()) {
        return false;
    }

    m_errorString.clear();
    m_warningString.clear();
    return true;
}

void ImageViewportPrivate::clearRequestIdentity()
{
    m_nextRequestId = 0;
    m_activeRequestId = 0;
    m_latestNonPlaybackRequestId = 0;
    m_activeRequestOrigin = DisplayRequestOrigin::None;
}

void ImageViewportPrivate::beginDisplayRequest(
    DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
{
    m_activeRequestId = ++m_nextRequestId;
    m_activeRequestOrigin = origin;
    if (rememberAsLatestNonPlayback) {
        m_latestNonPlaybackRequestId = m_activeRequestId;
    }
}

void ImageViewportPrivate::beginInitialDisplayRequest(bool rememberAsLatestNonPlayback)
{
    beginDisplayRequest(DisplayRequestOrigin::Initial, rememberAsLatestNonPlayback);
}

void ImageViewportPrivate::commitDisplayedRequestIdentity()
{
    m_displayedRequestId = m_activeRequestId;
    m_displayedPreparedPayloadId = m_pendingPreparedPayloadId;
}

void ImageViewportPrivate::beginPreparedPayloadIdentity()
{
    m_pendingRenderRequestId = m_activeRequestId;
    m_pendingPreparedPayloadId = m_activeRequestId == 0 ? 0 : ++m_nextPreparedPayloadId;
}

void ImageViewportPrivate::clearPendingRenderIdentity()
{
    m_pendingRenderRequestId = 0;
    m_pendingPreparedPayloadId = 0;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::ignoredNoRequest()
{
    setCommandDiagnostic(CommandReason::IgnoredNoRequest);
    return CommandOutcome::IgnoredNoRequest;
}

bool ImageViewportPrivate::hasActiveRequest() const
{
    return m_requestStatus != RequestStatus::NoRequest;
}

bool ImageViewportPrivate::hasReadyDisplay() const
{
    return hasDisplayableSequence()
        && (m_displayStatus == DisplayStatus::Ready || m_displayStatus == DisplayStatus::Retained)
        && m_displayedImageSize.isValid() && m_displayedImageSize.width() > 0.0
        && m_displayedImageSize.height() > 0.0;
}

bool ImageViewportPrivate::hasDisplayableSequence() const
{
    return m_sequence && m_sequence->isValid();
}

bool ImageViewportPrivate::hasStillSequence() const { return m_sequence && m_sequence->isStill(); }

bool ImageViewportPrivate::hasTimedSequence() const
{
    return m_sequence && m_sequence->isTimedList();
}

bool ImageViewportPrivate::hasProviderSequence() const
{
    return m_sequence && m_sequence->isProvider();
}

bool ImageViewportPrivate::hasGenerationTerminalProviderFailure() const
{
    return hasProviderSequence() && !m_providerSession
        && (m_requestStatus == RequestStatus::Unsupported
            || m_requestStatus == RequestStatus::Error);
}

bool ImageViewportPrivate::providerTimedPlaybackCapabilityKnownFalse() const
{
    return m_sequence
        && providerCapabilityKnownFalse(m_sequence->m_providerTimedPlaybackCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownFalse() const
{
    return m_sequence && providerCapabilityKnownFalse(m_sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownTrue() const
{
    return m_sequence && providerCapabilityKnownTrue(m_sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerPositionSeekCapabilityKnownFalse() const
{
    return m_sequence && providerCapabilityKnownFalse(m_sequence->m_providerPositionSeekCapability);
}

bool ImageViewportPrivate::providerKnownFactsTimedFrameCount() const
{
    return m_sequence && m_sequence->m_providerKnownFacts.isTimedFrameCount();
}

int ImageViewportPrivate::providerKnownFactsFrameCount() const
{
    return providerKnownFactsTimedFrameCount() ? m_sequence->m_providerKnownFacts.frameCount() : 0;
}

int ImageViewportPrivate::sequenceFrameCount() const
{
    return m_sequence ? m_sequence->frameCount() : 0;
}

int ImageViewportPrivate::sequenceFrameIndexForPosition(int position) const
{
    return m_sequence ? m_sequence->frameIndexForPosition(position) : -1;
}

int ImageViewportPrivate::sequenceFrameStartPosition(int frame) const
{
    return m_sequence ? m_sequence->frameStartPosition(frame) : -1;
}

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}
void ImageViewportPrivate::publishAcceptedTargetState(const QImage& providerImage)
{
    if (hasProviderSequence() && !providerImage.isNull()) {
        captureRenderFailureRetainedDisplay();
        m_pendingDisplayImage = providerImage;
        beginPreparedPayloadIdentity();
        if (itemBounds().isEmpty()) {
            publishRenderWaitingState();
        } else {
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::UploadPending;
            m_displayStatus
                = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
            m_renderCommitPending = false;
        }
        m_renderCommitPending = true;
        return;
    }
    if (itemBounds().isEmpty()) {
        publishRenderWaitingState();
    } else {
        publishSequenceReadyState(providerImage);
    }
}

void ImageViewportPrivate::publishSequenceReadyState(const QImage& providerImage)
{
    captureRenderFailureRetainedDisplay();
    m_requestStatus = RequestStatus::Ready;
    m_requestReason = RequestReason::Ready;
    m_displayStatus = DisplayStatus::Ready;
    m_renderCommitPending = true;
    beginPreparedPayloadIdentity();
    m_displayedFrame = m_currentFrame;
    m_displayedGeneration = m_sequenceGeneration;
    if (hasProviderSequence()) {
        m_displayedPosition = providerFrameStartPosition(m_currentFrame);
    } else {
        m_displayedPosition
            = hasTimedSequence() ? m_sequence->frameStartPosition(m_currentFrame) : -1;
    }
    m_displayedImageSize
        = hasProviderSequence() ? m_providerLogicalSize : m_sequence->logicalSize();
    if (hasProviderSequence()) {
        if (!providerImage.isNull()) {
            m_displayedImage = providerImage;
        } else if (!m_pendingDisplayImage.isNull()) {
            m_displayedImage = m_pendingDisplayImage;
        }
        m_pendingDisplayImage = {};
    } else {
        m_displayedImage = m_sequence ? m_sequence->frameImage(m_displayedFrame) : QImage();
        m_pendingDisplayImage = {};
    }
}

void ImageViewportPrivate::publishRenderWaitingState()
{
    m_requestStatus = RequestStatus::Loading;
    m_requestReason = RequestReason::RenderWaiting;
    m_displayStatus
        = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    m_renderCommitPending = false;
}
