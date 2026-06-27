#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <limits>

using namespace ImageViewportInternal;

namespace {
bool shouldPreservePlaybackPositionOnPlay(
    ImageViewport::PlaybackPhase phase, bool stopWhenRequestReady)
{
    return !stopWhenRequestReady
        && (phase == ImageViewport::PlaybackPhase::Playing
            || phase == ImageViewport::PlaybackPhase::Paused
            || phase == ImageViewport::PlaybackPhase::Waiting);
}
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::clearCommandImpl()
{
    const bool sequenceValueChanged = m_sequence != nullptr;
    const bool requestChanged = hasActiveRequest() || m_sequence;
    const bool displayChanged
        = m_displayStatus != DisplayStatus::Empty || m_displayedImageSize.isValid();
    const bool playbackChanged = m_playbackPhase != PlaybackPhase::Stopped;
    const bool diagnosticsValueChanged = !m_errorString.isEmpty() || !m_warningString.isEmpty();
    const QRectF oldContentRect = contentRect();
    const QRectF oldVisibleImageRect = visibleImageRect();
    closeProviderSession();
    m_sequence = nullptr;
    m_sequenceOwner.reset();
    ++m_sequenceGeneration;
    clearRequestIdentity();
    m_currentFrame = -1;
    m_requestedPosition = -1;
    m_playbackPosition = -1;
    m_latestNonPlaybackFrame = -1;
    m_latestNonPlaybackPosition = -1;
    m_currentProviderTargetKind = ProviderRequestTargetKind::Unknown;
    m_latestNonPlaybackProviderTargetKind = ProviderRequestTargetKind::Unknown;
    m_displayedFrame = -1;
    m_displayedPosition = -1;
    m_displayedGeneration = 0;
    m_displayedRequestId = 0;
    m_displayedPreparedPayloadId = 0;
    m_displayedImageSize = {};
    m_displayedImage = {};
    m_pendingDisplayImage = {};
    m_renderCommitPending = false;
    m_nextPreparedPayloadId = 0;
    clearPendingRenderIdentity();
    clearRenderFailureRetainedDisplay();
    m_requestStatus = RequestStatus::NoRequest;
    m_requestReason = RequestReason::NoRequest;
    m_displayStatus = DisplayStatus::Empty;
    m_playbackPhase = PlaybackPhase::Stopped;
    m_stopPlaybackWhenRequestReady = false;
    m_providerPlaybackStartPending = false;
    m_providerMetadataReady = false;
    m_providerTimedMetadata = false;
    m_providerLogicalSize = {};
    m_providerTimingIntervals = {};
    m_activeProviderMetadataToken = {};
    m_activeProviderFrameToken = {};
    m_activeProviderFrameRequestId = 0;
    m_activeProviderFrameFromPlayback = false;
    m_activeProviderFrameTargetKind = ProviderRequestTargetKind::Unknown;
    m_errorString.clear();
    m_warningString.clear();
    clearCommandDiagnosticForAcceptedCommand();
    if (requestChanged) {
        incrementRequestRevision();
    }
    if (displayChanged) {
        incrementDisplayRevision();
    }

    if (sequenceValueChanged) {
        emit q->sequenceChanged();
    }
    if (requestChanged) {
        emit q->requestStateChanged();
    }
    if (displayChanged) {
        emit q->displayStateChanged();
    }
    if (rectsDifferExactly(contentRect(), oldContentRect)
        || rectsDifferExactly(visibleImageRect(), oldVisibleImageRect)) {
        emit q->geometryStateChanged();
    }
    if (playbackChanged) {
        emit q->playbackPhaseChanged();
    }
    if (diagnosticsValueChanged) {
        emit q->diagnosticsChanged();
    }
    update();
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::playCommandImpl()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }
    if (hasGenerationTerminalProviderFailure()) {
        setCommandDiagnostic(CommandReason::UnsupportedRequest);
        return CommandOutcome::Unsupported;
    }

    if (hasProviderSequence() && m_providerMetadataReady && m_providerTimedMetadata
        && m_providerTimedPlaybackSupport) {
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(m_playbackPhase, m_stopPlaybackWhenRequestReady)
            && m_playbackPosition >= 0;
        m_stopPlaybackWhenRequestReady = false;
        if (m_requestStatus == RequestStatus::Unsupported
            || m_requestStatus == RequestStatus::Error) {
            int selectedFrame = m_currentFrame;
            if (selectedFrame < 0 || selectedFrame >= m_providerTimingIntervals.frameCount()) {
                selectedFrame = 0;
            }
            const int selectedPosition = providerFrameStartPosition(selectedFrame);
            clearCommandDiagnosticForAcceptedCommand();
            const bool diagnosticsValueChanged = clearDiagnostics();
            m_providerPlaybackStartPending = false;
            m_currentFrame = selectedFrame;
            m_requestedPosition = selectedPosition;
            m_playbackPosition = selectedPosition;
            m_currentProviderTargetKind = ProviderRequestTargetKind::Playback;
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::ProviderWaiting;
            m_displayStatus
                = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
            discardPendingRenderCommit();
            if (m_activeProviderFrameToken.isValid()) {
                queueProviderFrameRequest(selectedFrame, ProviderRequestTargetKind::Playback);
            } else if (!startProviderFrameRequest(
                           selectedFrame, ProviderRequestTargetKind::Playback)) {
                incrementRequestRevision();
                emit q->requestStateChanged();
                emit q->diagnosticsChanged();
                return CommandOutcome::Accepted;
            }
            setPlaybackPhase(PlaybackPhase::Waiting);
            incrementRequestRevision();
            emit q->requestStateChanged();
            if (diagnosticsValueChanged) {
                emit q->diagnosticsChanged();
            }
            return CommandOutcome::Accepted;
        }

        clearCommandDiagnosticForAcceptedCommand();
        if (!preservePlaybackPosition) {
            m_playbackPosition = m_requestedPosition >= 0
                ? m_requestedPosition
                : providerFrameStartPosition(m_currentFrame);
        }
        setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting
                                                                   : PlaybackPhase::Playing);
        return CommandOutcome::Accepted;
    }

    if (hasProviderSequence() && !m_providerMetadataReady
        && m_requestStatus == RequestStatus::Loading) {
        if (providerCapabilityKnownFalse(m_sequence->m_providerTimedPlaybackCapability)) {
            setCommandDiagnostic(CommandReason::UnsupportedRequest);
            return CommandOutcome::Unsupported;
        }

        clearCommandDiagnosticForAcceptedCommand();
        m_stopPlaybackWhenRequestReady = false;
        m_providerPlaybackStartPending = true;
        m_currentFrame = -1;
        m_requestedPosition = -1;
        m_playbackPosition = -1;
        m_currentProviderTargetKind = ProviderRequestTargetKind::Playback;
        setPlaybackPhase(PlaybackPhase::Waiting);
        incrementRequestRevision();
        emit q->requestStateChanged();
        return CommandOutcome::Accepted;
    }

    if (hasTimedSequence()) {
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(m_playbackPhase, m_stopPlaybackWhenRequestReady)
            && m_playbackPosition >= 0;
        clearCommandDiagnosticForAcceptedCommand();
        m_stopPlaybackWhenRequestReady = false;
        if (m_requestStatus == RequestStatus::Unsupported
            || m_requestStatus == RequestStatus::Error) {
            const QRectF oldContentRect = contentRect();
            const QRectF oldVisibleImageRect = visibleImageRect();
            const DisplayStatus oldDisplayStatus = m_displayStatus;
            const bool diagnosticsValueChanged = clearDiagnostics();
            publishAcceptedTargetState();
            m_playbackPosition = m_requestedPosition >= 0
                ? m_requestedPosition
                : m_sequence->frameStartPosition(m_currentFrame);
            setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting
                                                                       : PlaybackPhase::Playing);
            incrementRequestRevision();
            const bool displayValueChanged
                = m_displayStatus != oldDisplayStatus || m_displayStatus == DisplayStatus::Ready;
            if (displayValueChanged) {
                incrementDisplayRevision();
            }
            emit q->requestStateChanged();
            if (displayValueChanged) {
                emit q->displayStateChanged();
            }
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
        if (!preservePlaybackPosition) {
            m_playbackPosition = m_requestedPosition >= 0
                ? m_requestedPosition
                : m_sequence->frameStartPosition(m_currentFrame);
        }
        setPlaybackPhase(m_requestStatus == RequestStatus::Loading ? PlaybackPhase::Waiting
                                                                   : PlaybackPhase::Playing);
        return CommandOutcome::Accepted;
    }

    setCommandDiagnostic(CommandReason::UnsupportedRequest);
    return CommandOutcome::Unsupported;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::pauseCommandImpl()
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }

    clearCommandDiagnosticForAcceptedCommand();
    if (m_playbackPhase == PlaybackPhase::Playing || m_playbackPhase == PlaybackPhase::Waiting) {
        setPlaybackPhase(PlaybackPhase::Paused);
    }
    return CommandOutcome::Accepted;
}

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

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::seekCommandImpl(int frame)
{
    if (!hasActiveRequest()) {
        return ignoredNoRequest();
    }
    if (frame < 0) {
        setCommandDiagnostic(CommandReason::InvalidRequest);
        return CommandOutcome::Invalid;
    }
    if (hasGenerationTerminalProviderFailure()) {
        setCommandDiagnostic(CommandReason::UnsupportedRequest);
        return CommandOutcome::Unsupported;
    }

    if (hasDisplayableSequence()) {
        if (hasProviderSequence() && m_providerMetadataReady) {
            if (!m_providerFrameSeekSupport) {
                setCommandDiagnostic(CommandReason::UnsupportedRequest);
                return CommandOutcome::Unsupported;
            }
            const int maximumFrame
                = m_providerTimedMetadata ? m_providerTimingIntervals.frameCount() - 1 : 0;
            if (frame > maximumFrame) {
                setCommandDiagnostic(CommandReason::InvalidRequest);
                return CommandOutcome::Invalid;
            }

            clearCommandDiagnosticForAcceptedCommand();
            beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, true);
            m_providerPlaybackStartPending = false;
            m_currentFrame = frame;
            m_requestedPosition = providerFrameStartPosition(frame);
            m_playbackPosition = m_requestedPosition;
            m_currentProviderTargetKind = ProviderRequestTargetKind::Frame;
            m_latestNonPlaybackFrame = m_currentFrame;
            m_latestNonPlaybackPosition = m_requestedPosition;
            m_latestNonPlaybackProviderTargetKind = ProviderRequestTargetKind::Frame;
            m_requestStatus = RequestStatus::Loading;
            m_requestReason = RequestReason::ProviderWaiting;
            m_displayStatus
                = m_displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
            discardPendingRenderCommit();
            const bool diagnosticsValueChanged = clearDiagnostics();
            if (m_activeProviderFrameToken.isValid()) {
                queueProviderFrameRequest(frame, ProviderRequestTargetKind::Frame);
            } else if (!startProviderFrameRequest(frame, ProviderRequestTargetKind::Frame)) {
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

        if (hasProviderSequence() && !m_providerMetadataReady
            && m_requestStatus == RequestStatus::Loading) {
            if (providerCapabilityKnownFalse(m_sequence->m_providerFrameSeekCapability)) {
                setCommandDiagnostic(CommandReason::UnsupportedRequest);
                return CommandOutcome::Unsupported;
            }
            if (m_sequence->m_providerKnownFacts.isTimedFrameCount()
                && providerCapabilityKnownTrue(m_sequence->m_providerFrameSeekCapability)) {
                const int maximumFrame = m_sequence->m_providerKnownFacts.frameCount() - 1;
                if (frame > maximumFrame) {
                    setCommandDiagnostic(CommandReason::InvalidRequest);
                    return CommandOutcome::Invalid;
                }
            }

            clearCommandDiagnosticForAcceptedCommand();
            beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, true);
            m_providerPlaybackStartPending = false;
            m_currentFrame = frame;
            m_requestedPosition = -1;
            m_playbackPosition = -1;
            m_currentProviderTargetKind = ProviderRequestTargetKind::Frame;
            m_latestNonPlaybackFrame = m_currentFrame;
            m_latestNonPlaybackPosition = m_requestedPosition;
            m_latestNonPlaybackProviderTargetKind = ProviderRequestTargetKind::Frame;
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

        if (frame < 0 || frame >= m_sequence->frameCount()) {
            setCommandDiagnostic(CommandReason::InvalidRequest);
            return CommandOutcome::Invalid;
        }

        clearCommandDiagnosticForAcceptedCommand();
        beginDisplayRequest(DisplayRequestOrigin::ExplicitSeek, true);
        m_providerPlaybackStartPending = false;
        m_currentFrame = frame;
        m_requestedPosition = hasTimedSequence() ? m_sequence->frameStartPosition(frame) : -1;
        m_playbackPosition = m_requestedPosition;
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

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::resetViewCommandImpl()
{
    const bool changed = m_zoom != 1.0 || m_pan.x() != 0.0 || m_pan.y() != 0.0;
    m_zoom = 1.0;
    m_pan = {};
    if (changed) {
        notifyPresentationChanged(true);
    }
    clearCommandDiagnosticForAcceptedCommand();
    return CommandOutcome::Accepted;
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
