#include "viewportcontroller_p.h"

#include "imageviewport_p.h"

namespace {
using ImageViewportInternal::DisplayRequestTarget;

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

DisplayRequestTarget providerLatestNonPlaybackTarget(ImageViewportPrivate& viewport)
{
    DisplayRequestTarget target;
    target.frame = viewport.request.latestNonPlaybackRequest.target.frame;
    target.position = viewport.request.latestNonPlaybackRequest.target.position;
    target.providerTargetKind = viewport.request.latestNonPlaybackRequest.target.providerTargetKind;
    return target;
}

DisplayRequestTarget providerStopRestoreTarget(ImageViewportPrivate& viewport)
{
    DisplayRequestTarget target = providerLatestNonPlaybackTarget(viewport);
    if (target.frame < 0 && target.position >= 0) {
        target.frame = viewport.providerFrameIndexForPosition(target.position);
    }
    if (target.frame < 0 && target.position < 0
        && viewport.request.activeRequest.target.frame >= 0) {
        target.frame = viewport.request.activeRequest.target.frame;
        target.position = viewport.providerFrameStartPosition(target.frame);
    }
    if (target.position < 0 && target.frame >= 0) {
        target.position = viewport.providerFrameStartPosition(target.frame);
    }
    if (target.providerTargetKind == ImageViewportInternal::ProviderRequestTargetKind::Unknown
        && target.frame >= 0) {
        target.providerTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Frame;
    }
    return target;
}

void applyStopRestoreTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::StopRestore, true);
    viewport.request.activeRequest.target.frame = target.frame;
    viewport.request.activeRequest.target.position = target.position;
    viewport.request.playbackPosition = viewport.request.activeRequest.target.position;
}

void applyProviderStopRestoreTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    applyStopRestoreTarget(viewport, target);
    viewport.request.activeRequest.target.providerTargetKind = target.providerTargetKind;
}

bool providerStopRestoreTargetIsReadyDisplay(ImageViewportPrivate& viewport)
{
    return viewport.hasReadyDisplay()
        && viewport.request.displayedRequest.generation == viewport.request.sequenceGeneration
        && viewport.request.displayedRequest.request.target.frame
            == viewport.request.activeRequest.target.frame
        && viewport.request.displayedRequest.request.target.position
            == viewport.request.activeRequest.target.position;
}

bool renderAcknowledgementMatchesPending(
    ImageViewportPrivate& viewport, ViewportRenderAcknowledgement acknowledgement)
{
    return viewport.m_pendingRenderPayload.commitPending
        && acknowledgement.generation == viewport.m_pendingRenderPayload.generation
        && acknowledgement.generation == viewport.request.sequenceGeneration
        && acknowledgement.requestId == viewport.m_pendingRenderPayload.requestId
        && acknowledgement.preparedPayloadId == viewport.m_pendingRenderPayload.payloadId
        && acknowledgement.preparedPayloadId == viewport.request.activeRequest.preparedPayloadId;
}

void setPlaybackPhase(
    ImageViewportPrivate& viewport, ImageViewportInternal::ViewportChangeSet& changes,
    ImageViewport::PlaybackPhase phase)
{
    if (viewport.m_playbackPhase == phase) {
        return;
    }

    viewport.m_playbackPhase = phase;
    changes.playbackPhase = true;
}

void publishProviderStopRestoreLoading(ImageViewportPrivate& viewport)
{
    viewport.publishProviderFrameLoadingState();
}

void acceptExplicitSeekTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, true);
    viewport.m_providerPlaybackStartPending = false;
    viewport.request.activeRequest.target.frame = target.frame;
    viewport.request.activeRequest.target.position = target.position;
    viewport.request.playbackPosition = target.position;
    if (target.providerTargetKind != ImageViewportInternal::ProviderRequestTargetKind::Unknown) {
        viewport.request.activeRequest.target.providerTargetKind = target.providerTargetKind;
        viewport.request.latestNonPlaybackRequest.target.providerTargetKind
            = target.providerTargetKind;
    }
    viewport.request.latestNonPlaybackRequest.target.frame
        = viewport.request.activeRequest.target.frame;
    viewport.request.latestNonPlaybackRequest.target.position
        = viewport.request.activeRequest.target.position;
}

DisplayRequestTarget providerPlaybackStartTarget(ImageViewportPrivate& viewport)
{
    int selectedFrame = viewport.request.activeRequest.target.frame;
    if (selectedFrame < 0 || selectedFrame >= viewport.m_providerTimingIntervals.frameCount()) {
        selectedFrame = 0;
    }
    return DisplayRequestTarget { selectedFrame, viewport.providerFrameStartPosition(selectedFrame),
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

void applyProviderPlaybackStartTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.m_providerPlaybackStartPending = false;
    viewport.request.activeRequest.target.frame = target.frame;
    viewport.request.activeRequest.target.position = target.position;
    viewport.request.playbackPosition = target.position;
    viewport.request.activeRequest.target.providerTargetKind = target.providerTargetKind;
}

DisplayRequestTarget pendingProviderPlaybackTarget()
{
    return DisplayRequestTarget { -1, -1,
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

void applyPendingProviderPlaybackTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.m_providerPlaybackStartPending = true;
    viewport.request.activeRequest.target.frame = target.frame;
    viewport.request.activeRequest.target.position = target.position;
    viewport.request.playbackPosition = target.position;
    viewport.request.activeRequest.target.providerTargetKind = target.providerTargetKind;
}

template <typename FrameStartFor>
int playbackStartPosition(ImageViewportPrivate& viewport, FrameStartFor frameStartFor)
{
    const auto& target = viewport.request.activeRequest.target;
    return target.position >= 0 ? target.position : frameStartFor(target.frame);
}

template <typename FrameStartFor>
void seedPlaybackPosition(ImageViewportPrivate& viewport, FrameStartFor frameStartFor)
{
    viewport.request.playbackPosition = playbackStartPosition(viewport, frameStartFor);
}

ImageViewport::PlaybackPhase playbackPhaseForCurrentRequest(ImageViewportPrivate& viewport)
{
    return viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

ViewportCommandResult acceptProviderExplicitSeek(ImageViewportPrivate& viewport, int frame,
    int position, ImageViewportInternal::ProviderRequestTargetKind targetKind)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    acceptExplicitSeekTarget(viewport, DisplayRequestTarget { frame, position, targetKind });
    viewport.publishProviderFrameLoadingState();
    const bool diagnosticsValueChanged = viewport.clearDiagnostics();
    if (!viewport.dispatchProviderFrameRequest(frame, targetKind)) {
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
    acceptExplicitSeekTarget(viewport, DisplayRequestTarget { frame, position, targetKind });
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
    acceptExplicitSeekTarget(viewport, DisplayRequestTarget { frame, position });
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
    ++viewport.request.sequenceGeneration;
    viewport.clearRequestIdentity();
    viewport.request.activeRequest.target.frame = -1;
    viewport.request.activeRequest.target.position = -1;
    viewport.request.playbackPosition = -1;
    viewport.request.latestNonPlaybackRequest.target.frame = -1;
    viewport.request.latestNonPlaybackRequest.target.position = -1;
    viewport.request.activeRequest.target.providerTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.request.latestNonPlaybackRequest.target.providerTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.clearDisplayedDisplay();
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
            && viewport.request.playbackPosition >= 0;
        viewport.m_stopPlaybackWhenRequestReady = false;
        if (viewport.m_requestStatus == ImageViewport::RequestStatus::Unsupported
            || viewport.m_requestStatus == ImageViewport::RequestStatus::Error) {
            const DisplayRequestTarget target = providerPlaybackStartTarget(viewport);
            clearCommandDiagnosticForAcceptedCommand(viewport, result);
            const bool diagnosticsValueChanged = viewport.clearDiagnostics();
            applyProviderPlaybackStartTarget(viewport, target);
            viewport.publishProviderFrameLoadingState();
            if (!viewport.dispatchProviderFrameRequest(
                    target.frame, ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
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
            seedPlaybackPosition(viewport,
                [this](int frame) { return viewport.providerFrameStartPosition(frame); });
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
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
        applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
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
            && viewport.request.playbackPosition >= 0;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewport.m_stopPlaybackWhenRequestReady = false;
        if (viewport.m_requestStatus == ImageViewport::RequestStatus::Unsupported
            || viewport.m_requestStatus == ImageViewport::RequestStatus::Error) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus = viewport.m_displayStatus;
            const bool diagnosticsValueChanged = viewport.clearDiagnostics();
            viewport.publishAcceptedTargetState();
            seedPlaybackPosition(viewport,
                [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
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
            seedPlaybackPosition(viewport,
                [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
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
        && viewport.request.activeRequest.target.frame < 0
        && viewport.request.activeRequest.target.position < 0) {
        applyProviderStopRestoreTarget(viewport, providerLatestNonPlaybackTarget(viewport));
        viewport.m_providerPlaybackStartPending = false;
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }
    if (viewport.hasProviderSequence() && viewport.m_providerTimedMetadata
        && viewport.m_queuedProviderFrameRequest && viewport.m_queuedProviderFrameFromPlayback) {
        viewport.clearQueuedProviderFrameRequest();

        const DisplayRequestTarget restoredTarget = providerStopRestoreTarget(viewport);

        applyProviderStopRestoreTarget(viewport, restoredTarget);
        if (providerStopRestoreTargetIsReadyDisplay(viewport)) {
            viewport.publishReadyDisplayState();
        } else {
            publishProviderStopRestoreLoading(viewport);
            if (viewport.m_providerSession && viewport.request.activeRequest.target.frame >= 0
                && !viewport.startProviderFrameRequest(viewport.request.activeRequest.target.frame,
                    viewport.request.latestNonPlaybackRequest.target.providerTargetKind)) {
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

        const DisplayRequestTarget restoredTarget = providerStopRestoreTarget(viewport);

        applyProviderStopRestoreTarget(viewport, restoredTarget);
        if (providerStopRestoreTargetIsReadyDisplay(viewport)) {
            viewport.request.playbackPosition = viewport.request.activeRequest.target.position;
            viewport.publishReadyDisplayState();
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
        if (viewport.m_providerSession && viewport.request.activeRequest.target.frame >= 0) {
            if (!viewport.startProviderFrameRequest(viewport.request.activeRequest.target.frame,
                    viewport.request.latestNonPlaybackRequest.target.providerTargetKind)) {
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
        && viewport.request.latestNonPlaybackRequest.target.frame >= 0
        && viewport.request.activeRequest.target.frame
            != viewport.request.latestNonPlaybackRequest.target.frame) {
        const ImageViewport::DisplayStatus oldDisplayStatus = viewport.m_displayStatus;
        applyStopRestoreTarget(viewport,
            DisplayRequestTarget {
                viewport.request.latestNonPlaybackRequest.target.frame,
                viewport.request.latestNonPlaybackRequest.target.position });
        if (viewport.hasReadyDisplay()
            && viewport.request.displayedRequest.request.target.frame
                == viewport.request.activeRequest.target.frame
            && viewport.request.displayedRequest.request.target.position
                == viewport.request.activeRequest.target.position) {
            viewport.publishReadyDisplayState();
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

ImageViewportInternal::ViewportChangeSet ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (viewport.hasDisplayableSequence()
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        && (viewport.m_requestReason == ImageViewport::RequestReason::UploadPending
            || viewport.m_requestReason == ImageViewport::RequestReason::RenderWaiting)
        && !viewport.itemBounds().isEmpty()) {
        if (viewport.hasProviderSequence() && !viewport.m_pendingRenderPayload.image.isNull()) {
            changes.scheduleUpdate = true;
            return changes;
        }
        viewport.publishSequenceReadyState();
        if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
            setPlaybackPhase(viewport, changes,
                viewport.m_stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                        : ImageViewport::PlaybackPhase::Playing);
            viewport.m_stopPlaybackWhenRequestReady = false;
        }
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
        changes.displayState = true;
    } else if (viewport.hasProviderSequence()
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        && viewport.m_requestReason == ImageViewport::RequestReason::UploadPending
        && viewport.itemBounds().isEmpty() && !viewport.m_pendingRenderPayload.image.isNull()) {
        viewport.m_requestReason = ImageViewport::RequestReason::RenderWaiting;
        changes.requestRevision = true;
        changes.requestState = true;
        changes.displayRevision = true;
    } else {
        changes.displayRevision = true;
    }

    changes.geometryState = ImageViewportInternal::rectsDifferExactly(
        viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.scheduleUpdate = true;
    return changes;
}

FramePreparation::ProviderFrameState ViewportController::providerFramePreparationState() const
{
    ImageViewportInternal::PreparedPayload preparedPayload;
    preparedPayload.generation = viewport.request.sequenceGeneration;
    preparedPayload.requestId = viewport.request.activeRequest.identity.id;
    preparedPayload.payloadId
        = preparedPayload.requestId == 0 ? 0 : viewport.m_nextPreparedPayloadId + 1;
    return {
        viewport.m_providerMetadataReady,
        viewport.m_providerTimedMetadata,
        viewport.m_providerLogicalSize,
        viewport.m_providerTimingIntervals,
        viewport.request.activeRequest.target.frame,
        preparedPayload,
    };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (!admission.accepted()) {
        viewport.clearQueuedProviderFrameRequest();
        viewport.m_activeProviderFrameToken = {};
        viewport.m_activeProviderFrameRequestId = 0;
        viewport.m_activeProviderFrameFromPlayback = false;
        viewport.m_activeProviderFrameTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
        viewport.m_requestStatus = admission.status;
        viewport.m_requestReason = admission.reason;
        viewport.m_errorString = admission.diagnostic;
        setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
        changes.requestRevision = true;
        changes.requestState = true;
        changes.diagnostics = true;
        return changes;
    }

    const bool diagnosticsValueChanged = viewport.clearDiagnostics();
    viewport.m_activeProviderFrameToken = {};
    viewport.m_activeProviderFrameRequestId = 0;
    viewport.m_activeProviderFrameFromPlayback = false;
    viewport.m_activeProviderFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    viewport.publishAcceptedTargetState(admission.preparedPayload);
    if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Ready
        && !viewport.m_pendingRenderPayload.commitPending) {
        setPlaybackPhase(viewport, changes,
            viewport.m_stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                    : ImageViewport::PlaybackPhase::Playing);
        viewport.m_stopPlaybackWhenRequestReady = false;
    }
    changes.requestRevision = true;
    changes.displayRevision = true;
    changes.requestState = true;
    changes.displayState = true;
    changes.geometryState = ImageViewportInternal::rectsDifferExactly(
        viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.diagnostics = diagnosticsValueChanged;
    changes.scheduleUpdate = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    const ViewportProviderFrameTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewport.clearQueuedProviderFrameRequest();
    viewport.m_activeProviderFrameToken = {};
    viewport.m_activeProviderFrameRequestId = 0;
    viewport.m_activeProviderFrameFromPlayback = false;
    viewport.m_activeProviderFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_requestStatus = result.status;
    viewport.m_requestReason = result.reason;
    viewport.m_errorString
        = ImageViewportPrivate::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTerminalResult(
    const ViewportProviderMetadataTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewport.m_requestStatus = result.status;
    viewport.m_requestReason = result.reason;
    viewport.m_errorString
        = ImageViewportPrivate::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic);
    viewport.m_activeProviderMetadataToken = {};
    viewport.m_providerPlaybackStartPending = false;
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaiting()
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (viewport.m_requestStatus != ImageViewport::RequestStatus::Loading
        || viewport.m_requestReason == ImageViewport::RequestReason::ProviderWaiting) {
        return changes;
    }

    viewport.m_requestReason = ImageViewport::RequestReason::ProviderWaiting;
    changes.requestRevision = true;
    changes.requestState = true;
    return changes;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization()
{
    ViewportRenderSynchronization synchronization;
    synchronization.pendingProviderCommit = viewport.hasProviderSequence()
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        && (viewport.m_requestReason == ImageViewport::RequestReason::UploadPending
            || viewport.m_requestReason == ImageViewport::RequestReason::RenderWaiting)
        && viewport.m_pendingRenderPayload.commitPending
        && !viewport.m_pendingRenderPayload.image.isNull()
        && !viewport.itemBounds().isEmpty();
    synchronization.oldContentRect = viewport.contentRect();
    synchronization.oldVisibleImageRect = viewport.visibleImageRect();
    synchronization.oldDisplayStatus = viewport.m_displayStatus;
    if (synchronization.pendingProviderCommit) {
        synchronization.preparedPayload = viewport.m_pendingRenderPayload;
    } else if (viewport.m_pendingRenderPayload.commitPending && viewport.hasReadyDisplay()) {
        synchronization.preparedPayload = viewport.m_pendingRenderPayload;
        synchronization.preparedPayload.image = viewport.m_displayedImage;
    }
    return synchronization;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderCommit(
    ViewportRenderAcknowledgement acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (!renderedImagePresent) {
        return changes;
    }

    const bool renderMatchesPending = renderAcknowledgementMatchesPending(viewport, acknowledgement);
    if (renderMatchesPending && synchronization.pendingProviderCommit) {
        viewport.publishSequenceReadyState(synchronization.preparedPayload);
    }
    const bool resumePlaybackAfterCommit = renderMatchesPending
        && viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Ready;
    if (renderMatchesPending) {
        viewport.commitDisplayedRequestSnapshot();
        viewport.clearPendingRenderIdentity();
    }
    viewport.clearRenderFailureRetainedDisplay();
    if (resumePlaybackAfterCommit) {
        setPlaybackPhase(viewport, changes,
            viewport.m_stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                    : ImageViewport::PlaybackPhase::Playing);
        viewport.m_stopPlaybackWhenRequestReady = false;
    }
    if (synchronization.pendingProviderCommit) {
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
        changes.displayState = viewport.m_displayStatus != synchronization.oldDisplayStatus;
        changes.geometryState = ImageViewportInternal::rectsDifferExactly(
            viewport.contentRect(), synchronization.oldContentRect)
            || ImageViewportInternal::rectsDifferExactly(
                viewport.visibleImageRect(), synchronization.oldVisibleImageRect);
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderFailure(
    ViewportRenderAcknowledgement acknowledgement)
{
    ImageViewportInternal::ViewportChangeSet changes;
    const bool renderMatchesPending = renderAcknowledgementMatchesPending(viewport, acknowledgement);
    const bool pendingProviderCommit = viewport.hasProviderSequence()
        && viewport.m_requestStatus == ImageViewport::RequestStatus::Loading
        && (viewport.m_requestReason == ImageViewport::RequestReason::UploadPending
            || viewport.m_requestReason == ImageViewport::RequestReason::RenderWaiting)
        && viewport.m_pendingRenderPayload.commitPending
        && !viewport.m_pendingRenderPayload.image.isNull();
    if (!renderMatchesPending
        || (viewport.m_displayStatus != ImageViewport::DisplayStatus::Ready
            && !pendingProviderCommit)) {
        return changes;
    }

    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewport.m_displayStatus;

    viewport.m_requestStatus = ImageViewport::RequestStatus::Error;
    viewport.m_requestReason = ImageViewport::RequestReason::RenderFailure;
    viewport.clearPendingRenderIdentity();
    if (viewport.m_renderFailureRetainedDisplayValid) {
        viewport.m_displayStatus = ImageViewport::DisplayStatus::Retained;
        viewport.request.displayedRequest = viewport.request.renderFailureRetainedRequest;
        viewport.m_displayedImageSize = viewport.m_renderFailureRetainedImageSize;
        viewport.m_displayedImage = viewport.m_renderFailureRetainedImage;
    } else {
        viewport.m_displayStatus = ImageViewport::DisplayStatus::Empty;
        viewport.clearDisplayedDisplay();
    }
    viewport.clearRenderFailureRetainedDisplay();
    viewport.m_errorString = QStringLiteral("render commit failed");
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);

    changes.requestRevision = true;
    changes.displayRevision = true;
    changes.requestState = true;
    changes.displayState = viewport.m_displayStatus != oldDisplayStatus;
    changes.geometryState = ImageViewportInternal::rectsDifferExactly(
        viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.diagnostics = true;
    return changes;
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
