#include "viewportcontroller_p.h"

#include "imageviewport_p.h"

namespace {
void setCommandDiagnostic(ImageViewportPrivate& viewport, ViewportCommandResult& result,
    ImageViewport::CommandReason reason)
{
    viewport.m_commandReason = reason;
    result.changes.commandRevision = true;
}

void clearCommandDiagnosticForAcceptedCommand(
    ImageViewportPrivate& viewport, ViewportCommandResult& result)
{
    if (viewport.m_commandReason == ImageViewport::CommandReason::NoCommand) {
        return;
    }

    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::NoCommand);
}

bool shouldPreservePlaybackPositionOnPlay(
    ImageViewport::PlaybackPhase phase, bool stopWhenRequestReady)
{
    return !stopWhenRequestReady
        && (phase == ImageViewport::PlaybackPhase::Playing
            || phase == ImageViewport::PlaybackPhase::Paused
            || phase == ImageViewport::PlaybackPhase::Waiting);
}

void setPlaybackPhase(ImageViewportPrivate& viewport, ViewportCommandResult& result,
    ImageViewport::PlaybackPhase phase)
{
    if (viewport.m_playbackPhase == phase) {
        return;
    }

    viewport.m_playbackPhase = phase;
    result.changes.playbackPhase = true;
}

struct ProviderStopRestoreTarget {
    int frame = -1;
    int position = -1;
    ImageViewportInternal::ProviderRequestTargetKind targetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
};

ProviderStopRestoreTarget providerStopRestoreTarget(ImageViewportPrivate& viewport)
{
    ProviderStopRestoreTarget target;
    target.frame = viewport.m_latestNonPlaybackFrame;
    target.position = viewport.m_latestNonPlaybackPosition;
    if (target.frame < 0 && target.position >= 0) {
        target.frame = viewport.providerFrameIndexForPosition(target.position);
    }
    if (target.frame < 0 && target.position < 0 && viewport.m_currentFrame >= 0) {
        target.frame = viewport.m_currentFrame;
        target.position = viewport.providerFrameStartPosition(target.frame);
    }
    if (target.position < 0 && target.frame >= 0) {
        target.position = viewport.providerFrameStartPosition(target.frame);
    }
    target.targetKind = viewport.m_latestNonPlaybackProviderTargetKind;
    if (target.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Unknown
        && target.frame >= 0) {
        target.targetKind = ImageViewportInternal::ProviderRequestTargetKind::Frame;
    }
    return target;
}

void applyProviderStopRestoreTarget(ImageViewportPrivate& viewport, ProviderStopRestoreTarget target)
{
    viewport.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::StopRestore, true);
    viewport.m_currentFrame = target.frame;
    viewport.m_requestedPosition = target.position;
    viewport.m_playbackPosition = viewport.m_requestedPosition;
    viewport.m_currentProviderTargetKind = target.targetKind;
}

bool providerStopRestoreTargetIsReadyDisplay(ImageViewportPrivate& viewport)
{
    return viewport.hasReadyDisplay() && viewport.m_displayedGeneration == viewport.m_sequenceGeneration
        && viewport.m_displayedFrame == viewport.m_currentFrame
        && viewport.m_displayedPosition == viewport.m_requestedPosition;
}

void publishProviderStopRestoreLoading(ImageViewportPrivate& viewport)
{
    viewport.m_requestStatus = ImageViewport::RequestStatus::Loading;
    viewport.m_requestReason = ImageViewport::RequestReason::ProviderWaiting;
    viewport.m_displayStatus = viewport.m_displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    viewport.discardPendingRenderCommit();
}

void acceptExplicitSeekTarget(ImageViewportPrivate& viewport, int frame, int position,
    ImageViewportInternal::ProviderRequestTargetKind targetKind
    = ImageViewportInternal::ProviderRequestTargetKind::Unknown)
{
    viewport.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, true);
    viewport.m_providerPlaybackStartPending = false;
    viewport.m_currentFrame = frame;
    viewport.m_requestedPosition = position;
    viewport.m_playbackPosition = position;
    if (targetKind != ImageViewportInternal::ProviderRequestTargetKind::Unknown) {
        viewport.m_currentProviderTargetKind = targetKind;
        viewport.m_latestNonPlaybackProviderTargetKind = targetKind;
    }
    viewport.m_latestNonPlaybackFrame = viewport.m_currentFrame;
    viewport.m_latestNonPlaybackPosition = viewport.m_requestedPosition;
}

ViewportCommandResult acceptProviderExplicitSeek(ImageViewportPrivate& viewport, int frame,
    int position, ImageViewportInternal::ProviderRequestTargetKind targetKind)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    acceptExplicitSeekTarget(viewport, frame, position, targetKind);
    viewport.m_requestStatus = ImageViewport::RequestStatus::Loading;
    viewport.m_requestReason = ImageViewport::RequestReason::ProviderWaiting;
    viewport.m_displayStatus = viewport.m_displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    viewport.discardPendingRenderCommit();
    const bool diagnosticsValueChanged = viewport.clearDiagnostics();
    if (viewport.m_activeProviderFrameToken.isValid()) {
        viewport.queueProviderFrameRequest(frame, targetKind);
    } else if (!viewport.startProviderFrameRequest(frame, targetKind)) {
        result.changes.requestRevision = true;
        result.changes.displayRevision = true;
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.diagnostics = true;
        result.changes.scheduleUpdate = true;
        return result;
    }
    if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Playing) {
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
    }
    result.changes.requestRevision = true;
    result.changes.displayRevision = true;
    result.changes.requestState = true;
    result.changes.displayState = true;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportCommandResult acceptProviderPendingMetadataSeek(ImageViewportPrivate& viewport, int frame,
    int position, ImageViewportInternal::ProviderRequestTargetKind targetKind)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    acceptExplicitSeekTarget(viewport, frame, position, targetKind);
    viewport.m_requestStatus = ImageViewport::RequestStatus::Loading;
    viewport.m_requestReason = ImageViewport::RequestReason::ProviderWaiting;
    viewport.discardPendingRenderCommit();
    const bool diagnosticsValueChanged = viewport.clearDiagnostics();
    result.changes.requestRevision = true;
    result.changes.requestState = true;
    result.changes.diagnostics = diagnosticsValueChanged;
    return result;
}

ViewportCommandResult acceptBuiltInExplicitSeek(ImageViewportPrivate& viewport, int frame,
    int position)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    acceptExplicitSeekTarget(viewport, frame, position);
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const bool diagnosticsValueChanged = viewport.clearDiagnostics();
    viewport.publishAcceptedTargetState();
    if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Playing
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading) {
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
    }
    result.changes.requestRevision = true;
    result.changes.displayRevision = true;
    result.changes.requestState = true;
    result.changes.displayState = true;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}
}

ViewportController::ViewportController(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
}

ViewportCommandResult ViewportController::clear()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const bool sequenceValueChanged = viewport.m_sequence != nullptr;
    const bool requestChanged = viewport.hasActiveRequest() || viewport.m_sequence;
    const bool displayChanged = viewport.m_displayStatus != ImageViewport::DisplayStatus::Empty
        || viewport.m_displayedImageSize.isValid();
    const bool playbackChanged
        = viewport.m_playbackPhase != ImageViewport::PlaybackPhase::Stopped;
    const bool diagnosticsValueChanged
        = !viewport.m_errorString.isEmpty() || !viewport.m_warningString.isEmpty();
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    viewport.closeProviderSession();
    viewport.m_sequence = nullptr;
    viewport.m_sequenceOwner.reset();
    ++viewport.m_sequenceGeneration;
    viewport.clearRequestIdentity();
    viewport.m_currentFrame = -1;
    viewport.m_requestedPosition = -1;
    viewport.m_playbackPosition = -1;
    viewport.m_latestNonPlaybackFrame = -1;
    viewport.m_latestNonPlaybackPosition = -1;
    viewport.m_currentProviderTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_latestNonPlaybackProviderTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_displayedFrame = -1;
    viewport.m_displayedPosition = -1;
    viewport.m_displayedGeneration = 0;
    viewport.m_displayedRequestId = 0;
    viewport.m_displayedPreparedPayloadId = 0;
    viewport.m_displayedImageSize = {};
    viewport.m_displayedImage = {};
    viewport.m_pendingDisplayImage = {};
    viewport.m_renderCommitPending = false;
    viewport.m_nextPreparedPayloadId = 0;
    viewport.clearPendingRenderIdentity();
    viewport.clearRenderFailureRetainedDisplay();
    viewport.m_requestStatus = ImageViewport::RequestStatus::NoRequest;
    viewport.m_requestReason = ImageViewport::RequestReason::NoRequest;
    viewport.m_displayStatus = ImageViewport::DisplayStatus::Empty;
    viewport.m_playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    viewport.m_stopPlaybackWhenRequestReady = false;
    viewport.m_providerPlaybackStartPending = false;
    viewport.m_providerMetadataReady = false;
    viewport.m_providerTimedMetadata = false;
    viewport.m_providerLogicalSize = {};
    viewport.m_providerTimingIntervals = {};
    viewport.m_activeProviderMetadataToken = {};
    viewport.m_activeProviderFrameToken = {};
    viewport.m_activeProviderFrameRequestId = 0;
    viewport.m_activeProviderFrameFromPlayback = false;
    viewport.m_activeProviderFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_errorString.clear();
    viewport.m_warningString.clear();
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    result.changes.requestRevision = requestChanged;
    result.changes.displayRevision = displayChanged;
    result.changes.sequence = sequenceValueChanged;
    result.changes.requestState = requestChanged;
    result.changes.displayState = displayChanged;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.playbackPhase = playbackChanged;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportCommandResult ViewportController::play()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (viewport.hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && viewport.m_providerMetadataReady
        && viewport.m_providerTimedMetadata && viewport.m_providerTimedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(
                  viewport.m_playbackPhase, viewport.m_stopPlaybackWhenRequestReady)
            && viewport.m_playbackPosition >= 0;
        viewport.m_stopPlaybackWhenRequestReady = false;
        if (viewport.m_requestStatus == ImageViewport::RequestStatus::Unsupported
            || viewport.m_requestStatus == ImageViewport::RequestStatus::Error) {
            int selectedFrame = viewport.m_currentFrame;
            if (selectedFrame < 0
                || selectedFrame >= viewport.m_providerTimingIntervals.frameCount()) {
                selectedFrame = 0;
            }
            const int selectedPosition = viewport.providerFrameStartPosition(selectedFrame);
            clearCommandDiagnosticForAcceptedCommand(viewport, result);
            const bool diagnosticsValueChanged = viewport.clearDiagnostics();
            viewport.m_providerPlaybackStartPending = false;
            viewport.m_currentFrame = selectedFrame;
            viewport.m_requestedPosition = selectedPosition;
            viewport.m_playbackPosition = selectedPosition;
            viewport.m_currentProviderTargetKind
                = ImageViewportInternal::ProviderRequestTargetKind::Playback;
            viewport.m_requestStatus = ImageViewport::RequestStatus::Loading;
            viewport.m_requestReason = ImageViewport::RequestReason::ProviderWaiting;
            viewport.m_displayStatus = viewport.m_displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            viewport.discardPendingRenderCommit();
            if (viewport.m_activeProviderFrameToken.isValid()) {
                viewport.queueProviderFrameRequest(
                    selectedFrame, ImageViewportInternal::ProviderRequestTargetKind::Playback);
            } else if (!viewport.startProviderFrameRequest(
                           selectedFrame,
                           ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
                result.changes.requestRevision = true;
                result.changes.requestState = true;
                result.changes.diagnostics = true;
                return result;
            }
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
            result.changes.requestRevision = true;
            result.changes.requestState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return result;
        }

        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        if (!preservePlaybackPosition) {
            viewport.m_playbackPosition = viewport.m_requestedPosition >= 0
                ? viewport.m_requestedPosition
                : viewport.providerFrameStartPosition(viewport.m_currentFrame);
        }
        setPlaybackPhase(viewport, result,
            viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
                ? ImageViewport::PlaybackPhase::Waiting
                : ImageViewport::PlaybackPhase::Playing);
        return result;
    }

    if (viewport.hasProviderSequence() && !viewport.m_providerMetadataReady
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerTimedPlaybackCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewport.m_stopPlaybackWhenRequestReady = false;
        viewport.m_providerPlaybackStartPending = true;
        viewport.m_currentFrame = -1;
        viewport.m_requestedPosition = -1;
        viewport.m_playbackPosition = -1;
        viewport.m_currentProviderTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Playback;
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (viewport.hasTimedSequence()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(
                  viewport.m_playbackPhase, viewport.m_stopPlaybackWhenRequestReady)
            && viewport.m_playbackPosition >= 0;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewport.m_stopPlaybackWhenRequestReady = false;
        if (viewport.m_requestStatus == ImageViewport::RequestStatus::Unsupported
            || viewport.m_requestStatus == ImageViewport::RequestStatus::Error) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus = viewport.m_displayStatus;
            const bool diagnosticsValueChanged = viewport.clearDiagnostics();
            viewport.publishAcceptedTargetState();
            viewport.m_playbackPosition = viewport.m_requestedPosition >= 0
                ? viewport.m_requestedPosition
                : viewport.sequenceFrameStartPosition(viewport.m_currentFrame);
            setPlaybackPhase(viewport, result,
                viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
                    ? ImageViewport::PlaybackPhase::Waiting
                    : ImageViewport::PlaybackPhase::Playing);
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewport.m_displayStatus != oldDisplayStatus
                || viewport.m_displayStatus == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(
                      viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.diagnostics = diagnosticsValueChanged;
            result.changes.scheduleUpdate = true;
            return result;
        }
        if (!preservePlaybackPosition) {
            viewport.m_playbackPosition = viewport.m_requestedPosition >= 0
                ? viewport.m_requestedPosition
                : viewport.sequenceFrameStartPosition(viewport.m_currentFrame);
        }
        setPlaybackPhase(viewport, result,
            viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
                ? ImageViewport::PlaybackPhase::Waiting
                : ImageViewport::PlaybackPhase::Playing);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::pause()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewport.m_playbackPhase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportCommandResult ViewportController::stop()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    viewport.m_stopPlaybackWhenRequestReady = false;
    if (viewport.hasProviderSequence() && !viewport.m_providerMetadataReady
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        && (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewport.m_currentFrame < 0 && viewport.m_requestedPosition < 0) {
        viewport.beginDisplayRequest(
            ImageViewportInternal::DisplayRequestOrigin::StopRestore, true);
        viewport.m_currentFrame = viewport.m_latestNonPlaybackFrame;
        viewport.m_requestedPosition = viewport.m_latestNonPlaybackPosition;
        viewport.m_playbackPosition = viewport.m_requestedPosition;
        viewport.m_currentProviderTargetKind = viewport.m_latestNonPlaybackProviderTargetKind;
        viewport.m_providerPlaybackStartPending = false;
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }
    if (viewport.hasProviderSequence() && viewport.m_providerTimedMetadata
        && viewport.m_queuedProviderFrameRequest && viewport.m_queuedProviderFrameFromPlayback) {
        viewport.clearQueuedProviderFrameRequest();

        const ProviderStopRestoreTarget restoredTarget = providerStopRestoreTarget(viewport);

        applyProviderStopRestoreTarget(viewport, restoredTarget);
        if (providerStopRestoreTargetIsReadyDisplay(viewport)) {
            viewport.m_requestStatus = ImageViewport::RequestStatus::Ready;
            viewport.m_requestReason = ImageViewport::RequestReason::Ready;
            viewport.m_displayStatus = ImageViewport::DisplayStatus::Ready;
        } else {
            publishProviderStopRestoreLoading(viewport);
            if (viewport.m_providerSession && viewport.m_currentFrame >= 0
                && !viewport.startProviderFrameRequest(
                    viewport.m_currentFrame, viewport.m_latestNonPlaybackProviderTargetKind)) {
                result.changes.requestRevision = true;
                result.changes.requestState = true;
                result.changes.diagnostics = true;
                return result;
            }
        }
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }
    if (viewport.hasProviderSequence() && viewport.m_providerTimedMetadata
        && viewport.m_activeProviderFrameFromPlayback) {
        if (viewport.m_providerSession) {
            viewport.cancelProviderRequest(viewport.m_activeProviderFrameToken);
        }
        viewport.m_activeProviderFrameToken = {};
        viewport.m_activeProviderFrameRequestId = 0;
        viewport.m_activeProviderFrameFromPlayback = false;
        viewport.m_activeProviderFrameTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;

        const ProviderStopRestoreTarget restoredTarget = providerStopRestoreTarget(viewport);

        applyProviderStopRestoreTarget(viewport, restoredTarget);
        if (providerStopRestoreTargetIsReadyDisplay(viewport)) {
            viewport.m_playbackPosition = viewport.m_requestedPosition;
            viewport.m_requestStatus = ImageViewport::RequestStatus::Ready;
            viewport.m_requestReason = ImageViewport::RequestReason::Ready;
            viewport.m_displayStatus = ImageViewport::DisplayStatus::Ready;
            const bool diagnosticsValueChanged = viewport.clearDiagnostics();
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
            result.changes.requestRevision = true;
            result.changes.displayRevision = true;
            result.changes.requestState = true;
            result.changes.displayState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return result;
        }
        publishProviderStopRestoreLoading(viewport);
        const bool diagnosticsValueChanged = viewport.clearDiagnostics();
        if (viewport.m_providerSession && viewport.m_currentFrame >= 0) {
            if (!viewport.startProviderFrameRequest(
                    viewport.m_currentFrame, viewport.m_latestNonPlaybackProviderTargetKind)) {
                result.changes.requestRevision = true;
                result.changes.requestState = true;
                result.changes.diagnostics = true;
                return result;
            }
        }
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        return result;
    }
    if (viewport.hasTimedSequence()
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        && viewport.m_requestReason == ImageViewport::RequestReason::RenderWaiting
        && (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewport.m_latestNonPlaybackFrame >= 0
        && viewport.m_currentFrame != viewport.m_latestNonPlaybackFrame) {
        const ImageViewport::DisplayStatus oldDisplayStatus = viewport.m_displayStatus;
        viewport.beginDisplayRequest(
            ImageViewportInternal::DisplayRequestOrigin::StopRestore, true);
        viewport.m_currentFrame = viewport.m_latestNonPlaybackFrame;
        viewport.m_requestedPosition = viewport.m_latestNonPlaybackPosition;
        viewport.m_playbackPosition = viewport.m_requestedPosition;
        if (viewport.hasReadyDisplay() && viewport.m_displayedFrame == viewport.m_currentFrame
            && viewport.m_displayedPosition == viewport.m_requestedPosition) {
            viewport.m_requestStatus = ImageViewport::RequestStatus::Ready;
            viewport.m_requestReason = ImageViewport::RequestReason::Ready;
            viewport.m_displayStatus = ImageViewport::DisplayStatus::Ready;
        } else {
            viewport.publishRenderWaitingState();
        }
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        result.changes.requestRevision = true;
        result.changes.displayRevision = viewport.m_displayStatus != oldDisplayStatus;
        result.changes.requestState = true;
        result.changes.displayState = result.changes.displayRevision;
        result.changes.scheduleUpdate = true;
        return result;
    }
    setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
    return result;
}

ViewportCommandResult ViewportController::seek(int frame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (viewport.hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasDisplayableSequence()) {
        if (viewport.hasProviderSequence() && viewport.m_providerMetadataReady) {
            if (!viewport.m_providerFrameSeekSupport) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            const int maximumFrame = viewport.m_providerTimedMetadata
                ? viewport.m_providerTimingIntervals.frameCount() - 1
                : 0;
            if (frame > maximumFrame) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }

            return acceptProviderExplicitSeek(viewport, frame,
                viewport.providerFrameStartPosition(frame),
                ImageViewportInternal::ProviderRequestTargetKind::Frame);
        }

        if (viewport.hasProviderSequence() && !viewport.m_providerMetadataReady
            && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading) {
            if (viewport.providerFrameSeekCapabilityKnownFalse()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (viewport.providerKnownFactsTimedFrameCount()
                && viewport.providerFrameSeekCapabilityKnownTrue()) {
                const int maximumFrame = viewport.providerKnownFactsFrameCount() - 1;
                if (frame > maximumFrame) {
                    ViewportCommandResult result;
                    result.outcome = ImageViewport::CommandOutcome::Invalid;
                    setCommandDiagnostic(
                        viewport, result, ImageViewport::CommandReason::InvalidRequest);
                    return result;
                }
            }

            return acceptProviderPendingMetadataSeek(
                viewport, frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame);
        }

        if (frame >= viewport.sequenceFrameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptBuiltInExplicitSeek(viewport, frame,
            viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(frame) : -1);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (viewport.hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && !viewport.m_providerMetadataReady
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerPositionSeekCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        return acceptProviderPendingMetadataSeek(viewport, -1, milliseconds,
            ImageViewportInternal::ProviderRequestTargetKind::Position);
    }

    if (viewport.hasProviderSequence() && viewport.m_providerMetadataReady
        && viewport.m_providerTimedMetadata) {
        if (!viewport.m_providerPositionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = viewport.providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptProviderExplicitSeek(viewport, frame, milliseconds,
            ImageViewportInternal::ProviderRequestTargetKind::Position);
    }

    if (viewport.hasTimedSequence()) {
        const int frame = viewport.sequenceFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptBuiltInExplicitSeek(viewport, frame, milliseconds);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::resetView()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const bool changed = viewport.m_zoom != 1.0 || viewport.m_pan.x() != 0.0
        || viewport.m_pan.y() != 0.0;
    viewport.m_zoom = 1.0;
    viewport.m_pan = {};
    if (changed) {
        result.changes.presentation = true;
        result.changes.displayRevision = true;
        result.changes.geometryState = viewport.hasReadyDisplay() && !viewport.itemBounds().isEmpty();
        result.changes.scheduleUpdate = true;
    }
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    return result;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::advancePlaybackForTest(int elapsedMilliseconds)
{
    viewport.advancePlaybackForTestImpl(elapsedMilliseconds);
}

void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    viewport.setNextProviderRequestTokenForTestImpl(token);
}

bool ViewportController::hasPendingRenderCommitForTest() const
{
    return viewport.hasPendingRenderCommitForTestImpl();
}

quint64 ViewportController::activeRequestIdForTest() const
{
    return viewport.activeRequestIdForTestImpl();
}

quint64 ViewportController::displayedRequestIdForTest() const
{
    return viewport.displayedRequestIdForTestImpl();
}

quint64 ViewportController::pendingRenderPayloadIdForTest() const
{
    return viewport.pendingRenderPayloadIdForTestImpl();
}
#endif

void ImageViewportPrivate::applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes)
{
    if (changes.displayRevision) {
        incrementDisplayRevision();
    }
    if (changes.requestRevision) {
        incrementRequestRevision();
    }
    if (changes.commandRevision) {
        ++m_commandRevision;
        emit q->commandRevisionChanged();
        emit q->commandStateChanged();
    }

    if (changes.sequence) {
        emit q->sequenceChanged();
    }
    if (changes.requestState) {
        emit q->requestStateChanged();
    }
    if (changes.displayState) {
        emit q->displayStateChanged();
    }
    if (changes.geometryState) {
        emit q->geometryStateChanged();
    }
    if (changes.playbackPhase) {
        emit q->playbackPhaseChanged();
    }
    if (changes.diagnostics) {
        emit q->diagnosticsChanged();
    }
    if (changes.presentation) {
        emit q->presentationChanged();
    }
    if (changes.scheduleUpdate) {
        update();
    }
}

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.clear();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.play();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.pause();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.stop();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seek(frame);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seekToPosition(milliseconds);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    const ViewportCommandResult result = controller.resetView();
    applyControllerChanges(result.changes);
    return result.outcome;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    controller.advancePlaybackForTest(elapsedMilliseconds);
}

void ImageViewportPrivate::setNextProviderRequestTokenForTest(quint64 token)
{
    controller.setNextProviderRequestTokenForTest(token);
}

bool ImageViewportPrivate::hasPendingRenderCommitForTest() const
{
    return controller.hasPendingRenderCommitForTest();
}

quint64 ImageViewportPrivate::activeRequestIdForTest() const
{
    return controller.activeRequestIdForTest();
}

quint64 ImageViewportPrivate::displayedRequestIdForTest() const
{
    return controller.displayedRequestIdForTest();
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTest() const
{
    return controller.pendingRenderPayloadIdForTest();
}
#endif
