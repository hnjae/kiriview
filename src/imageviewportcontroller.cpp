#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <limits>

using namespace ImageViewportInternal;

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::stopCommandImpl()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    clearCommandDiagnosticForAcceptedCommand();
    m_stopPlaybackWhenRequestReady = false;
    if (hasProviderSequence() && !m_providerMetadataReady
        && m_requestStatus == RequestStatus::Loading
        && (m_playbackPhase == PlaybackPhase::Waiting || m_playbackPhase == PlaybackPhase::Paused)
        && m_currentFrame < 0 && m_requestedPosition < 0) {
        beginDisplayRequest(DisplayRequestOrigin::StopRestore, true);
        m_currentFrame = m_latestNonPlaybackFrame;
        m_requestedPosition = m_latestNonPlaybackPosition;
        m_playbackPosition = m_requestedPosition;
        m_currentProviderTargetKind = m_latestNonPlaybackProviderTargetKind;
        m_providerPlaybackStartPending = false;
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        return CommandOutcome::Accepted;
    }
    if (hasProviderSequence() && m_providerTimedMetadata && m_queuedProviderFrameRequest
        && m_queuedProviderFrameFromPlayback) {
        clearQueuedProviderFrameRequest();

        int restoredFrame = m_latestNonPlaybackFrame;
        int restoredPosition = m_latestNonPlaybackPosition;
        if (restoredFrame < 0 && restoredPosition >= 0) {
            restoredFrame = providerFrameIndexForPosition(restoredPosition);
        }
        if (restoredFrame < 0 && restoredPosition < 0 && m_currentFrame >= 0) {
            restoredFrame = m_currentFrame;
            restoredPosition = providerFrameStartPosition(restoredFrame);
        }
        if (restoredPosition < 0 && restoredFrame >= 0) {
            restoredPosition = providerFrameStartPosition(restoredFrame);
        }
        ProviderRequestTargetKind restoredTargetKind = m_latestNonPlaybackProviderTargetKind;
        if (restoredTargetKind == ProviderRequestTargetKind::Unknown && restoredFrame >= 0) {
            restoredTargetKind = ProviderRequestTargetKind::Frame;
        }

        beginDisplayRequest(DisplayRequestOrigin::StopRestore, true);
        m_currentFrame = restoredFrame;
        m_requestedPosition = restoredPosition;
        m_playbackPosition = m_requestedPosition;
        m_currentProviderTargetKind = restoredTargetKind;
        if (hasReadyDisplay() && m_displayedGeneration == m_sequenceGeneration
            && m_displayedFrame == m_currentFrame && m_displayedPosition == m_requestedPosition) {
            m_requestStatus = RequestStatus::Ready;
            m_requestReason = RequestReason::Ready;
            m_displayStatus = DisplayStatus::Ready;
        } else {
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::ProviderWaiting;
            m_displayStatus
                = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
            discardPendingRenderCommit();
            if (m_providerSession && m_currentFrame >= 0
                && !startProviderFrameRequest(
                    m_currentFrame, m_latestNonPlaybackProviderTargetKind)) {
                incrementRequestRevision();
                emit q->requestStateChanged();
                emit q->diagnosticsChanged();
                return CommandOutcome::Accepted;
            }
        }
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        return CommandOutcome::Accepted;
    }
    if (hasProviderSequence() && m_providerTimedMetadata && m_activeProviderFrameFromPlayback) {
        if (m_providerSession) {
            cancelProviderRequest(m_activeProviderFrameToken);
        }
        m_activeProviderFrameToken = {};
        m_activeProviderFrameRequestId = 0;
        m_activeProviderFrameFromPlayback = false;
        m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;

        int restoredFrame = m_latestNonPlaybackFrame;
        int restoredPosition = m_latestNonPlaybackPosition;
        if (restoredFrame < 0 && restoredPosition >= 0) {
            restoredFrame = providerFrameIndexForPosition(restoredPosition);
        }
        if (restoredFrame < 0 && restoredPosition < 0 && m_currentFrame >= 0) {
            restoredFrame = m_currentFrame;
            restoredPosition = providerFrameStartPosition(restoredFrame);
        }
        if (restoredPosition < 0 && restoredFrame >= 0) {
            restoredPosition = providerFrameStartPosition(restoredFrame);
        }
        ProviderRequestTargetKind restoredTargetKind = m_latestNonPlaybackProviderTargetKind;
        if (restoredTargetKind == ProviderRequestTargetKind::Unknown && restoredFrame >= 0) {
            restoredTargetKind = ProviderRequestTargetKind::Frame;
        }

        beginDisplayRequest(DisplayRequestOrigin::StopRestore, true);
        m_currentFrame = restoredFrame;
        m_requestedPosition = restoredPosition;
        m_playbackPosition = m_requestedPosition;
        m_currentProviderTargetKind = restoredTargetKind;
        if (hasReadyDisplay() && m_displayedGeneration == m_sequenceGeneration
            && m_displayedFrame == m_currentFrame && m_displayedPosition == m_requestedPosition) {
            m_playbackPosition = m_requestedPosition;
            m_requestStatus = RequestStatus::Ready;
            m_requestReason = RequestReason::Ready;
            m_displayStatus = DisplayStatus::Ready;
            const bool diagnosticsValueChanged = clearDiagnostics();
            setPlaybackPhase(PlaybackPhase::Stopped);
            incrementRequestRevision();
            incrementDisplayRevision();
            emit q->requestStateChanged();
            emit q->displayStateChanged();
            if (diagnosticsValueChanged) {
                emit q->diagnosticsChanged();
            }
            return CommandOutcome::Accepted;
        }
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus
            = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        discardPendingRenderCommit();
        const bool diagnosticsValueChanged = clearDiagnostics();
        if (m_providerSession && m_currentFrame >= 0) {
            if (!startProviderFrameRequest(m_currentFrame, m_latestNonPlaybackProviderTargetKind)) {
                incrementRequestRevision();
                emit q->requestStateChanged();
                emit q->diagnosticsChanged();
                return CommandOutcome::Accepted;
            }
        }
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        emit q->requestStateChanged();
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        return CommandOutcome::Accepted;
    }
    if (hasTimedSequence() && m_requestStatus == RequestStatus::Loading
        && m_requestReason == RequestReason::RenderWaiting
        && (m_playbackPhase == PlaybackPhase::Waiting || m_playbackPhase == PlaybackPhase::Paused)
        && m_latestNonPlaybackFrame >= 0 && m_currentFrame != m_latestNonPlaybackFrame) {
        const DisplayStatus oldDisplayStatus = m_displayStatus;
        beginDisplayRequest(DisplayRequestOrigin::StopRestore, true);
        m_currentFrame = m_latestNonPlaybackFrame;
        m_requestedPosition = m_latestNonPlaybackPosition;
        m_playbackPosition = m_requestedPosition;
        if (hasReadyDisplay() && m_displayedFrame == m_currentFrame
            && m_displayedPosition == m_requestedPosition) {
            m_requestStatus = RequestStatus::Ready;
            m_requestReason = RequestReason::Ready;
            m_displayStatus = DisplayStatus::Ready;
        } else {
            publishRenderWaitingState();
        }
        setPlaybackPhase(PlaybackPhase::Stopped);
        incrementRequestRevision();
        if (m_displayStatus != oldDisplayStatus) {
            incrementDisplayRevision();
            emit q->displayStateChanged();
        }
        emit q->requestStateChanged();
        update();
        return CommandOutcome::Accepted;
    }
    setPlaybackPhase(PlaybackPhase::Stopped);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::seekToPositionCommandImpl(
    int milliseconds)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    if (milliseconds < 0) {
        setCommandDiagnostic(CommandReason::InvalidRequest);
        return CommandOutcome::Invalid;
    }
    if (hasGenerationTerminalProviderFailure()) {
        setCommandDiagnostic(CommandReason::UnsupportedRequest);
        return CommandOutcome::Unsupported;
    }

    if (hasProviderSequence() && !m_providerMetadataReady
        && m_requestStatus == RequestStatus::Loading) {
        if (providerCapabilityKnownFalse(m_sequence->m_providerPositionSeekCapability)) {
            setCommandDiagnostic(CommandReason::UnsupportedRequest);
            return CommandOutcome::Unsupported;
        }

        clearCommandDiagnosticForAcceptedCommand();
        beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, true);
        m_providerPlaybackStartPending = false;
        m_currentFrame = -1;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        m_currentProviderTargetKind = ProviderRequestTargetKind::Position;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        m_latestNonPlaybackProviderTargetKind = ProviderRequestTargetKind::Position;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        discardPendingRenderCommit();
        const bool diagnosticsValueChanged = clearDiagnostics();
        incrementRequestRevision();
        emit q->requestStateChanged();
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        return CommandOutcome::Accepted;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        if (!m_providerPositionSeekSupport) {
            setCommandDiagnostic(CommandReason::UnsupportedRequest);
            return CommandOutcome::Unsupported;
        }
        const int frame = providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, true);
        m_providerPlaybackStartPending = false;
        m_currentFrame = frame;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        m_currentProviderTargetKind = ProviderRequestTargetKind::Position;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        m_latestNonPlaybackProviderTargetKind = ProviderRequestTargetKind::Position;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus
            = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        discardPendingRenderCommit();
        const bool diagnosticsValueChanged = clearDiagnostics();
        if (m_activeProviderFrameToken.isValid()) {
            queueProviderFrameRequest(frame, ProviderRequestTargetKind::Position);
        } else if (!startProviderFrameRequest(frame, ProviderRequestTargetKind::Position)) {
            incrementRequestRevision();
            incrementDisplayRevision();
            emit q->requestStateChanged();
            emit q->displayStateChanged();
            emit q->diagnosticsChanged();
            update();
            return CommandOutcome::Accepted;
        }
        if (m_playbackPhase == PlaybackPhase::Playing) {
            setPlaybackPhase(PlaybackPhase::Waiting);
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        emit q->requestStateChanged();
        emit q->displayStateChanged();
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        update();
        return CommandOutcome::Accepted;
    }

    if (hasTimedSequence()) {
        const int frame = m_sequence->frameIndexForPosition(milliseconds);
        if (frame < 0) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, true);
        m_providerPlaybackStartPending = false;
        m_currentFrame = frame;
        m_requestedPosition = milliseconds;
        m_playbackPosition = milliseconds;
        m_latestNonPlaybackFrame = m_currentFrame;
        m_latestNonPlaybackPosition = m_requestedPosition;
        const QRectF oldContentRect = contentRect();
        const QRectF oldVisibleImageRect = visibleImageRect();
        const bool diagnosticsValueChanged = clearDiagnostics();
        publishAcceptedTargetState();
        if (m_playbackPhase == PlaybackPhase::Playing
            && m_requestStatus == RequestStatus::Loading) {
            setPlaybackPhase(PlaybackPhase::Waiting);
        }
        incrementRequestRevision();
        incrementDisplayRevision();
        emit q->requestStateChanged();
        emit q->displayStateChanged();
        if (rectsDifferExactly(contentRect(), oldContentRect)
            || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
            emit q->geometryStateChanged();
        }
        if (diagnosticsValueChanged) {
            emit q->diagnosticsChanged();
        }
        update();
        return CommandOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return CommandOutcome::Unsupported;
}

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    if (m_playbackPhase != PlaybackPhase::Playing || elapsedMilliseconds <= 0) {
        return;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata) {
        const int duration = totalDuration();
        const int previousFrame = m_currentFrame;
        int nextPlaybackPosition = m_playbackPosition < 0
            ? providerFrameStartPosition(m_currentFrame)
            : m_playbackPosition;
        nextPlaybackPosition += elapsedMilliseconds;

        int nextFrame = -1;
        int nextRequestedPosition = -1;
        if (nextPlaybackPosition >= duration) {
            if (m_looping) {
                const int wrappedPosition = duration > 0 ? nextPlaybackPosition % duration : 0;
                nextFrame = providerFrameIndexForPosition(wrappedPosition);
                if (nextFrame < 0) {
                    return;
                }
                nextPlaybackPosition = wrappedPosition;
                nextRequestedPosition = providerFrameStartPosition(nextFrame);
            } else {
                nextFrame = frameCount() - 1;
                nextRequestedPosition = providerFrameStartPosition(nextFrame);
                nextPlaybackPosition = duration;
                m_stopPlaybackWhenRequestReady = true;
            }
        } else {
            nextFrame = providerFrameIndexForPosition(nextPlaybackPosition);
            if (nextFrame < 0) {
                return;
            }
            nextRequestedPosition = providerFrameStartPosition(nextFrame);
        }

        m_playbackPosition = nextPlaybackPosition;
        if (nextFrame == previousFrame && m_requestStatus == RequestStatus::Ready) {
            if (m_stopPlaybackWhenRequestReady) {
                setPlaybackPhase(PlaybackPhase::Stopped);
                m_stopPlaybackWhenRequestReady = false;
            }
            return;
        }

        beginDisplayRequest(DisplayRequestOrigin::Playback, false);
        m_currentFrame = nextFrame;
        m_requestedPosition = nextRequestedPosition;
        m_currentProviderTargetKind = ProviderRequestTargetKind::Playback;
        m_requestStatus = RequestStatus::Loading;
        m_requestReason = RequestReason::ProviderWaiting;
        m_displayStatus
            = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
        discardPendingRenderCommit();
        const bool diagnosticsValueChanged = clearDiagnostics();
        if (m_activeProviderFrameToken.isValid()) {
            queueProviderFrameRequest(nextFrame, ProviderRequestTargetKind::Playback);
        } else if (!startProviderFrameRequest(nextFrame, ProviderRequestTargetKind::Playback)) {
            incrementRequestRevision();
            if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
                incrementDisplayRevision();
            }
            emit q->requestStateChanged();
            emit q->displayStateChanged();
            emit q->diagnosticsChanged();
            update();
            return;
        }
        setPlaybackPhase(PlaybackPhase::Waiting);
        incrementRequestRevision();
        if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
            incrementDisplayRevision();
        }
        emit q->requestStateChanged();
        emit q->displayStateChanged();
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
    int nextPlaybackPosition = m_playbackPosition < 0
        ? m_sequence->frameStartPosition(m_currentFrame)
        : m_playbackPosition;
    nextPlaybackPosition += elapsedMilliseconds;

    if (nextPlaybackPosition >= totalDuration) {
        if (m_looping) {
            const int wrappedPosition
                = totalDuration > 0 ? nextPlaybackPosition % totalDuration : 0;
            const int wrappedFrame = m_sequence->frameIndexForPosition(wrappedPosition);
            if (wrappedFrame < 0) {
                return;
            }

            beginDisplayRequest(DisplayRequestOrigin::Playback, false);
            m_currentFrame = wrappedFrame;
            m_requestedPosition = m_sequence->frameStartPosition(wrappedFrame);
            m_playbackPosition = wrappedPosition;
            const QRectF oldContentRect = contentRect();
            const QRectF oldVisibleImageRect = visibleImageRect();
            publishAcceptedTargetState();
            setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting
                                                                       : PlaybackPhase::Playing);
            incrementRequestRevision();
            if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
                incrementDisplayRevision();
            }
            emit q->requestStateChanged();
            emit q->displayStateChanged();
            if (rectsDifferExactly(contentRect(), oldContentRect)
                || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
                emit q->geometryStateChanged();
            }
            update();
            return;
        }

        const int finalFrame = m_sequence->frameCount() - 1;
        beginDisplayRequest(DisplayRequestOrigin::Playback, false);
        m_currentFrame = finalFrame;
        m_requestedPosition = m_sequence->frameStartPosition(finalFrame);
        m_playbackPosition = totalDuration;
        const QRectF oldContentRect = contentRect();
        const QRectF oldVisibleImageRect = visibleImageRect();
        publishAcceptedTargetState();
        m_stopPlaybackWhenRequestReady = m_requestStatus == RequestStatus::Loading;
        setPlaybackPhase(
            m_stopPlaybackWhenRequestReady ? PlaybackPhase::Waiting : PlaybackPhase::Stopped);
        incrementRequestRevision();
        if (m_currentFrame != previousFrame || m_displayStatus != DisplayStatus::Ready) {
            incrementDisplayRevision();
        }
        emit q->requestStateChanged();
        emit q->displayStateChanged();
        if (rectsDifferExactly(contentRect(), oldContentRect)
            || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
            emit q->geometryStateChanged();
        }
        update();
        return;
    }

    const int nextFrame = m_sequence->frameIndexForPosition(nextPlaybackPosition);
    if (nextFrame < 0) {
        return;
    }

    m_playbackPosition = nextPlaybackPosition;
    if (nextFrame == m_currentFrame) {
        return;
    }

    beginDisplayRequest(DisplayRequestOrigin::Playback, false);
    m_currentFrame = nextFrame;
    m_requestedPosition = m_sequence->frameStartPosition(nextFrame);
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    publishAcceptedTargetState();
    setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting
                                                               : PlaybackPhase::Playing);
    incrementRequestRevision();
    incrementDisplayRevision();
    emit q->requestStateChanged();
    emit q->displayStateChanged();
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
