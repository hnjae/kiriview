#include "viewportcontroller_p.h"

#include "imageviewport_p.h"

#include <cmath>
#include <limits>

namespace {
using ImageViewportInternal::DisplayRequestTarget;

enum class ExplicitSeekMaterialization {
    ProviderReady,
    ProviderPendingMetadata,
    BuiltIn,
};

struct PlaybackAdvanceTarget
{
    DisplayRequestTarget displayTarget;
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
            const int wrappedPosition
                = totalDuration > 0 ? nextPlaybackPosition % totalDuration : 0;
            const int wrappedFrame = frameIndexFor(wrappedPosition);
            if (wrappedFrame < 0) {
                return target;
            }
            target.displayTarget.frame = wrappedFrame;
            target.playbackPosition = wrappedPosition;
            target.displayTarget.position = frameStartFor(wrappedFrame);
            target.looped = true;
            target.valid = true;
            return target;
        }

        const int finalFrame = frameCount - 1;
        target.displayTarget.frame = finalFrame;
        target.displayTarget.position = frameStartFor(finalFrame);
        target.playbackPosition = totalDuration;
        target.reachedEnd = true;
        target.valid = true;
        return target;
    }

    const int nextFrame = frameIndexFor(nextPlaybackPosition);
    if (nextFrame < 0) {
        return target;
    }
    target.displayTarget.frame = nextFrame;
    target.displayTarget.position = frameStartFor(nextFrame);
    target.playbackPosition = nextPlaybackPosition;
    target.valid = true;
    return target;
}

void setCommandDiagnostic(ImageViewportPrivate& viewport, ViewportCommandResult& result,
    ImageViewport::CommandReason reason)
{
    viewport.request.setCommandDiagnostic(reason);
    result.changes.commandRevision = true;
}

void clearCommandDiagnosticForAcceptedCommand(
    ImageViewportPrivate& viewport, ViewportCommandResult& result)
{
    result.changes.commandRevision = viewport.request.clearCommandDiagnosticForAcceptedCommand()
        || result.changes.commandRevision;
}

bool shouldPreservePlaybackPositionOnPlay(
    ImageViewport::PlaybackPhase phase, bool stopWhenRequestReady)
{
    return !stopWhenRequestReady
        && (phase == ImageViewport::PlaybackPhase::Playing
            || phase == ImageViewport::PlaybackPhase::Paused
            || phase == ImageViewport::PlaybackPhase::Waiting);
}

bool activeProviderFrameTokenMatchesActiveRequest(
    const ImageViewportPrivate& viewport, ImageSequenceProviderRequestToken token)
{
    return viewport.provider.activeFrameToken.isValid()
        && token == viewport.provider.activeFrameToken
        && token == viewport.request.activeRequest.providerFrameToken
        && viewport.provider.activeFrameRequestId == viewport.request.activeRequest.identity.id;
}

ViewportProviderFrameTerminalResult frameTerminalResultFor(
    const ViewportProviderTerminalEvent& event)
{
    switch (event.kind) {
    case ViewportProviderTerminalEvent::Kind::Unsupported:
        return {
            ImageViewport::RequestStatus::Unsupported,
            event.unsupportedCause
                    == ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
                ? ImageViewport::RequestReason::UnsupportedRequest
                : ImageViewport::RequestReason::PayloadRejection,
            event.diagnostic,
            QStringLiteral("provider unsupported"),
        };
    case ViewportProviderTerminalEvent::Kind::Cancellation:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider cancelled request"),
        };
    case ViewportProviderTerminalEvent::Kind::Failure:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider failure"),
        };
    }

    return {};
}

ViewportProviderMetadataTerminalResult metadataTerminalResultFor(
    const ViewportProviderTerminalEvent& event)
{
    switch (event.kind) {
    case ViewportProviderTerminalEvent::Kind::Unsupported:
        return {
            ImageViewport::RequestStatus::Unsupported,
            ImageViewport::RequestReason::UnsupportedRequest,
            event.diagnostic,
            QStringLiteral("provider unsupported"),
        };
    case ViewportProviderTerminalEvent::Kind::Cancellation:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider cancelled request"),
        };
    case ViewportProviderTerminalEvent::Kind::Failure:
        return {
            ImageViewport::RequestStatus::Error,
            ImageViewport::RequestReason::ProviderFailure,
            event.diagnostic,
            QStringLiteral("provider failure"),
        };
    }

    return {};
}

void setPlaybackPhase(ImageViewportPrivate& viewport, ViewportCommandResult& result,
    ImageViewport::PlaybackPhase phase)
{
    if (viewport.request.playbackPhase == phase) {
        return;
    }

    viewport.request.playbackPhase = phase;
    result.changes.playbackPhase = true;
}

void appendProviderFrameQueueResult(
    ViewportProviderFrameTransportEffect& effect, ViewportProviderFrameQueueResult queue)
{
    effect.cancelToken = queue.cancelToken;
    effect.scheduleFlush = queue.scheduleFlush;
}

void appendProviderFrameStartResult(ViewportProviderFrameTransportEffect& effect,
    const ViewportProviderFrameRequestStartResult& start)
{
    effect.closeSession = start.closeSession;
    effect.sessionClose = start.sessionClose;
    effect.sendCommand = start.sendCommand;
    effect.command = start.command;
}

void appendProviderMetadataStartResult(ViewportProviderMetadataTransportEffect& effect,
    const ViewportProviderMetadataRequestStartResult& start)
{
    effect.closeSession = start.closeSession;
    effect.sessionClose = start.sessionClose;
    effect.sendCommand = start.sendCommand;
    effect.token = start.token;
}

void clearQueuedProviderFrameRequest(ImageViewportPrivate& viewport)
{
    viewport.provider.queuedFrameRequest = false;
    viewport.provider.queuedFrameGeneration = 0;
    viewport.provider.queuedFrameRequestId = 0;
    viewport.provider.queuedFrame = -1;
    viewport.provider.queuedPosition = -1;
    viewport.provider.queuedFrameFromPlayback = false;
    viewport.provider.queuedFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
}

void publishProviderTokenExhaustion(ImageViewportPrivate& viewport)
{
    clearQueuedProviderFrameRequest(viewport);
    viewport.provider.activeMetadataToken = {};
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.request.providerPlaybackStartPending = false;
    viewport.request.stopPlaybackWhenRequestReady = false;
    viewport.request.status = ImageViewport::RequestStatus::Error;
    viewport.request.reason = ImageViewport::RequestReason::ProviderFailure;
    viewport.request.errorString = QStringLiteral("provider request token exhausted");
    viewport.request.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
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

void beginStopRestoreDisplayRequest(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.request.beginDisplayRequest(
        ImageViewportInternal::DisplayRequestOrigin::StopRestore, target, true);
    viewport.request.playbackPosition = target.position;
}

void applyStopRestoreTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    beginStopRestoreDisplayRequest(viewport, target);
}

void applyProviderStopRestoreTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    beginStopRestoreDisplayRequest(viewport, target);
}

void beginAcceptedDisplayRequest(ImageViewportPrivate& viewport,
    ImageViewportInternal::DisplayRequestOrigin origin, DisplayRequestTarget target,
    bool rememberAsLatestNonPlayback)
{
    viewport.request.beginDisplayRequest(origin, target, rememberAsLatestNonPlayback);
}

bool stopRestoreTargetIsReadyDisplay(ImageViewportPrivate& viewport)
{
    return viewport.hasReadyDisplay()
        && viewport.display.displayedRequest.generation == viewport.request.sequenceGeneration
        && viewport.display.displayedRequest.request.target.frame
        == viewport.request.activeRequest.target.frame
        && viewport.display.displayedRequest.request.target.position
        == viewport.request.activeRequest.target.position;
}

void publishProviderStopRestoreLoading(ImageViewportPrivate& viewport);

enum class StopRestoreWaitingState {
    ProviderLoading,
    RenderWaiting,
};

struct StopRestorePublication
{
    bool readyDisplay = false;
    ImageViewport::DisplayStatus oldDisplayStatus = ImageViewport::DisplayStatus::Empty;
};

enum class StopRestorePlanKind {
    None,
    ProviderPendingMetadata,
    ProviderQueuedPlayback,
    ProviderActivePlayback,
    BuiltInRenderWait,
};

struct StopRestorePlan
{
    StopRestorePlanKind kind = StopRestorePlanKind::None;
    DisplayRequestTarget target;
};

StopRestorePlan stopRestorePlanFor(ImageViewportPrivate& viewport)
{
    if (viewport.hasProviderSequence() && !viewport.provider.metadataReady
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewport.request.activeRequest.target.frame < 0
        && viewport.request.activeRequest.target.position < 0) {
        return { StopRestorePlanKind::ProviderPendingMetadata,
            providerLatestNonPlaybackTarget(viewport) };
    }
    if (viewport.hasProviderSequence() && viewport.provider.timedMetadata
        && viewport.provider.queuedFrameRequest && viewport.provider.queuedFrameFromPlayback) {
        return { StopRestorePlanKind::ProviderQueuedPlayback, providerStopRestoreTarget(viewport) };
    }
    if (viewport.hasProviderSequence() && viewport.provider.timedMetadata
        && viewport.provider.activeFrameFromPlayback) {
        return { StopRestorePlanKind::ProviderActivePlayback, providerStopRestoreTarget(viewport) };
    }
    if (viewport.hasTimedSequence()
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && viewport.request.reason == ImageViewport::RequestReason::RenderWaiting
        && (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewport.request.latestNonPlaybackRequest.target.frame >= 0
        && viewport.request.activeRequest.target.frame
            != viewport.request.latestNonPlaybackRequest.target.frame) {
        return { StopRestorePlanKind::BuiltInRenderWait,
            DisplayRequestTarget { viewport.request.latestNonPlaybackRequest.target.frame,
                viewport.request.latestNonPlaybackRequest.target.position } };
    }
    return {};
}

StopRestorePublication publishStopRestoreTarget(ImageViewportPrivate& viewport,
    DisplayRequestTarget target, StopRestoreWaitingState waitingState)
{
    StopRestorePublication publication;
    publication.oldDisplayStatus = viewport.display.status;
    if (waitingState == StopRestoreWaitingState::ProviderLoading) {
        applyProviderStopRestoreTarget(viewport, target);
    } else {
        applyStopRestoreTarget(viewport, target);
    }

    publication.readyDisplay = stopRestoreTargetIsReadyDisplay(viewport);
    if (publication.readyDisplay) {
        viewport.publishReadyDisplayState();
    } else if (waitingState == StopRestoreWaitingState::ProviderLoading) {
        publishProviderStopRestoreLoading(viewport);
    } else {
        viewport.publishRenderWaitingState();
    }
    return publication;
}

void completeStopRestoreRequest(ImageViewportPrivate& viewport, ViewportCommandResult& result)
{
    setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
    result.changes.requestRevision = true;
    result.changes.requestState = true;
}

bool renderAcknowledgementMatchesPending(
    ImageViewportPrivate& viewport, ViewportRenderAcknowledgement acknowledgement)
{
    return viewport.display.pendingRenderPayload.commitPending
        && acknowledgement.preparedPayload.generation
        == viewport.display.pendingRenderPayload.generation
        && acknowledgement.preparedPayload.generation == viewport.request.sequenceGeneration
        && acknowledgement.preparedPayload.requestId
        == viewport.display.pendingRenderPayload.requestId
        && acknowledgement.preparedPayload.payloadId
        == viewport.display.pendingRenderPayload.payloadId
        && acknowledgement.preparedPayload.payloadId
        == viewport.request.activeRequest.preparedPayloadId;
}

void setPlaybackPhase(ImageViewportPrivate& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, ImageViewport::PlaybackPhase phase)
{
    if (viewport.request.playbackPhase == phase) {
        return;
    }

    viewport.request.playbackPhase = phase;
    changes.playbackPhase = true;
}

void publishProviderStopRestoreLoading(ImageViewportPrivate& viewport)
{
    viewport.publishProviderFrameLoadingState();
}

bool appendProviderStopRestoreFrameStart(
    ViewportController& controller, ImageViewportPrivate& viewport, ViewportCommandResult& result)
{
    if (!viewport.provider.session || viewport.request.activeRequest.target.frame < 0) {
        return true;
    }

    DisplayRequestTarget target = viewport.request.activeRequest.target;
    target.providerTargetKind = viewport.request.latestNonPlaybackRequest.target.providerTargetKind;
    const ViewportProviderFrameRequestStartResult start
        = controller.startProviderFrameRequest({ target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (start.accepted) {
        return true;
    }

    result.changes.requestRevision = true;
    result.changes.requestState = true;
    result.changes.diagnostics = true;
    return false;
}

bool applyStopRestorePlan(ViewportController& controller, ImageViewportPrivate& viewport,
    StopRestorePlan plan, ViewportCommandResult& result)
{
    switch (plan.kind) {
    case StopRestorePlanKind::ProviderPendingMetadata:
        applyProviderStopRestoreTarget(viewport, plan.target);
        viewport.request.providerPlaybackStartPending = false;
        completeStopRestoreRequest(viewport, result);
        return true;
    case StopRestorePlanKind::ProviderQueuedPlayback: {
        clearQueuedProviderFrameRequest(viewport);

        const StopRestorePublication publication = publishStopRestoreTarget(
            viewport, plan.target, StopRestoreWaitingState::ProviderLoading);
        if (!publication.readyDisplay) {
            if (!appendProviderStopRestoreFrameStart(controller, viewport, result)) {
                return true;
            }
        }
        completeStopRestoreRequest(viewport, result);
        return true;
    }
    case StopRestorePlanKind::ProviderActivePlayback: {
        if (viewport.provider.session) {
            result.providerFrameTransport.cancelToken = viewport.provider.activeFrameToken;
        }
        viewport.provider.activeFrameToken = {};
        viewport.provider.activeFrameRequestId = 0;
        viewport.provider.activeFrameFromPlayback = false;
        viewport.provider.activeFrameTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;

        const StopRestorePublication publication = publishStopRestoreTarget(
            viewport, plan.target, StopRestoreWaitingState::ProviderLoading);
        if (publication.readyDisplay) {
            const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
            completeStopRestoreRequest(viewport, result);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return true;
        }
        const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
        if (!appendProviderStopRestoreFrameStart(controller, viewport, result)) {
            return true;
        }
        completeStopRestoreRequest(viewport, result);
        result.changes.diagnostics = diagnosticsValueChanged;
        return true;
    }
    case StopRestorePlanKind::BuiltInRenderWait: {
        const StopRestorePublication publication = publishStopRestoreTarget(
            viewport, plan.target, StopRestoreWaitingState::RenderWaiting);
        completeStopRestoreRequest(viewport, result);
        result.changes.displayRevision = viewport.display.status != publication.oldDisplayStatus;
        result.changes.displayState = result.changes.displayRevision;
        result.changes.scheduleUpdate = true;
        return true;
    }
    case StopRestorePlanKind::None:
        return false;
    }
    return false;
}

void acceptExplicitSeekTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    beginAcceptedDisplayRequest(
        viewport, ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, target, true);
    viewport.request.providerPlaybackStartPending = false;
    viewport.request.playbackPosition = target.position;
}

DisplayRequestTarget providerPlaybackStartTarget(ImageViewportPrivate& viewport)
{
    int selectedFrame = viewport.request.activeRequest.target.frame;
    if (selectedFrame < 0 || selectedFrame >= viewport.provider.timingIntervals.frameCount()) {
        selectedFrame = 0;
    }
    return DisplayRequestTarget { selectedFrame, viewport.providerFrameStartPosition(selectedFrame),
        ImageViewportInternal::ProviderRequestTargetKind::Playback };
}

void applyProviderPlaybackStartTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    viewport.request.providerPlaybackStartPending = false;
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
    viewport.request.providerPlaybackStartPending = true;
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
    return viewport.request.status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

ImageViewport::PlaybackPhase playbackAdvancePhaseForRequest(
    ImageViewport::RequestStatus requestStatus, bool reachedEnd)
{
    if (reachedEnd && requestStatus != ImageViewport::RequestStatus::Loading) {
        return ImageViewport::PlaybackPhase::Stopped;
    }
    return requestStatus == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

void applyPlaybackTarget(ImageViewportPrivate& viewport, DisplayRequestTarget target)
{
    beginAcceptedDisplayRequest(
        viewport, ImageViewportInternal::DisplayRequestOrigin::Playback, target, false);
}

void applyPlaybackAdvancePhase(ImageViewportPrivate& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, const PlaybackAdvanceTarget& target)
{
    if (target.reachedEnd) {
        viewport.request.stopPlaybackWhenRequestReady
            = viewport.request.status == ImageViewport::RequestStatus::Loading;
    }
    setPlaybackPhase(viewport, changes,
        playbackAdvancePhaseForRequest(viewport.request.status, target.reachedEnd));
}

void appendPlaybackRequestChange(ImageViewportPrivate& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, int previousFrame)
{
    changes.requestRevision = true;
    if (viewport.request.activeRequest.target.frame != previousFrame
        || viewport.display.status != ImageViewport::DisplayStatus::Ready) {
        changes.displayRevision = true;
    }
    changes.requestState = true;
    changes.displayState = true;
}

ViewportCommandResult acceptExplicitSeek(ViewportController& controller,
    ImageViewportPrivate& viewport, DisplayRequestTarget target,
    ExplicitSeekMaterialization materialization)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    acceptExplicitSeekTarget(viewport, target);

    switch (materialization) {
    case ExplicitSeekMaterialization::ProviderReady: {
        viewport.publishProviderFrameLoadingState();
        const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
        const ViewportProviderFrameDispatchResult dispatch
            = controller.dispatchProviderFrameRequest({ target });
        result.providerFrameTransport = dispatch.transport;
        if (!dispatch.accepted) {
            result.changes.requestRevision = true;
            result.changes.displayRevision = true;
            result.changes.requestState = true;
            result.changes.displayState = true;
            result.changes.diagnostics = true;
            result.changes.scheduleUpdate = true;
            return result;
        }
        if (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Playing) {
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
    case ExplicitSeekMaterialization::ProviderPendingMetadata: {
        viewport.request.status = ImageViewport::RequestStatus::Loading;
        viewport.request.reason = ImageViewport::RequestReason::ProviderWaiting;
        viewport.discardPendingRenderCommit();
        const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        return result;
    }
    case ExplicitSeekMaterialization::BuiltIn: {
        const QRectF oldContentRect = viewport.contentRect();
        const QRectF oldVisibleImageRect = viewport.visibleImageRect();
        const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
        viewport.publishAcceptedTargetState();
        if (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Playing
            && viewport.request.status == ImageViewport::RequestStatus::Loading) {
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
    const bool sequenceValueChanged = viewport.request.sequence != nullptr;
    const bool requestChanged = viewport.hasActiveRequest() || viewport.request.sequence;
    const bool displayChanged = viewport.display.status != ImageViewport::DisplayStatus::Empty
        || viewport.display.displayedImageSize.isValid();
    const bool playbackChanged
        = viewport.request.playbackPhase != ImageViewport::PlaybackPhase::Stopped;
    const bool diagnosticsValueChanged
        = !viewport.request.errorString.isEmpty() || !viewport.request.warningString.isEmpty();
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const bool closeProviderSession = viewport.provider.session != nullptr;
    result.providerFrameTransport.sessionClose = handleProviderSessionClose();
    result.providerFrameTransport.closeSession = closeProviderSession;
    viewport.request.sequence = nullptr;
    viewport.request.sequenceOwner.reset();
    ++viewport.request.sequenceGeneration;
    viewport.request.clearDisplayRequests();
    viewport.display.clearDisplayedDisplay();
    viewport.display.nextPreparedPayloadId = 0;
    viewport.display.clearPendingRenderPayload();
    viewport.display.clearRenderFailureRetainedDisplay();
    viewport.request.status = ImageViewport::RequestStatus::NoRequest;
    viewport.request.reason = ImageViewport::RequestReason::NoRequest;
    viewport.display.status = ImageViewport::DisplayStatus::Empty;
    viewport.request.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    viewport.request.stopPlaybackWhenRequestReady = false;
    viewport.request.providerPlaybackStartPending = false;
    viewport.provider.metadataReady = false;
    viewport.provider.timedMetadata = false;
    viewport.provider.logicalSize = {};
    viewport.provider.timingIntervals = {};
    viewport.provider.activeMetadataToken = {};
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.request.errorString.clear();
    viewport.request.warningString.clear();
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

    if (viewport.hasProviderSequence() && viewport.provider.metadataReady
        && viewport.provider.timedMetadata && viewport.provider.timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(
                  viewport.request.playbackPhase, viewport.request.stopPlaybackWhenRequestReady)
            && viewport.request.playbackPosition >= 0;
        viewport.request.stopPlaybackWhenRequestReady = false;
        if (viewport.request.status == ImageViewport::RequestStatus::Unsupported
            || viewport.request.status == ImageViewport::RequestStatus::Error) {
            const DisplayRequestTarget target = providerPlaybackStartTarget(viewport);
            clearCommandDiagnosticForAcceptedCommand(viewport, result);
            const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
            applyProviderPlaybackStartTarget(viewport, target);
            viewport.publishProviderFrameLoadingState();
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest({ target });
            result.providerFrameTransport = dispatch.transport;
            if (!dispatch.accepted) {
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
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.providerFrameStartPosition(frame); });
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    if (viewport.hasProviderSequence() && !viewport.provider.metadataReady
        && viewport.request.status == ImageViewport::RequestStatus::Loading) {
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
        viewport.request.stopPlaybackWhenRequestReady = false;
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
                  viewport.request.playbackPhase, viewport.request.stopPlaybackWhenRequestReady)
            && viewport.request.playbackPosition >= 0;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewport.request.stopPlaybackWhenRequestReady = false;
        if (viewport.request.status == ImageViewport::RequestStatus::Unsupported
            || viewport.request.status == ImageViewport::RequestStatus::Error) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus = viewport.display.status;
            const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
            viewport.publishAcceptedTargetState();
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
            result.changes.requestRevision = true;
            const bool displayValueChanged = viewport.display.status != oldDisplayStatus
                || viewport.display.status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.diagnostics = diagnosticsValueChanged;
            result.changes.scheduleUpdate = true;
            return result;
        }
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
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
    if (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewport.request.playbackPhase = ImageViewport::PlaybackPhase::Paused;
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
    viewport.request.stopPlaybackWhenRequestReady = false;
    const StopRestorePlan stopRestorePlan = stopRestorePlanFor(viewport);
    if (applyStopRestorePlan(*this, viewport, stopRestorePlan, result)) {
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
        if (viewport.hasProviderSequence() && viewport.provider.metadataReady) {
            if (!viewport.provider.frameSeekSupport) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            const int maximumFrame = viewport.provider.timedMetadata
                ? viewport.provider.timingIntervals.frameCount() - 1
                : 0;
            if (frame > maximumFrame) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget { frame, viewport.providerFrameStartPosition(frame),
                    ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderReady);
        }

        if (viewport.hasProviderSequence() && !viewport.provider.metadataReady
            && viewport.request.status == ImageViewport::RequestStatus::Loading) {
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

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget {
                    frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderPendingMetadata);
        }

        if (frame >= viewport.sequenceFrameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget { frame,
                viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(frame) : -1 },
            ExplicitSeekMaterialization::BuiltIn);
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

    if (viewport.hasProviderSequence() && !viewport.provider.metadataReady
        && viewport.request.status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerPositionSeekCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            ExplicitSeekMaterialization::ProviderPendingMetadata);
    }

    if (viewport.hasProviderSequence() && viewport.provider.metadataReady
        && viewport.provider.timedMetadata) {
        if (!viewport.provider.positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = viewport.providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            ExplicitSeekMaterialization::ProviderReady);
    }

    if (viewport.hasTimedSequence()) {
        const int frame = viewport.sequenceFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport, DisplayRequestTarget { frame, milliseconds },
            ExplicitSeekMaterialization::BuiltIn);
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
    const bool changed = viewport.presentation.zoom != 1.0 || viewport.presentation.pan.x() != 0.0
        || viewport.presentation.pan.y() != 0.0;
    viewport.presentation.zoom = 1.0;
    viewport.presentation.pan = {};
    if (changed) {
        result.changes.presentation = true;
        result.changes.displayRevision = true;
        result.changes.geometryState
            = viewport.hasReadyDisplay() && !viewport.itemBounds().isEmpty();
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
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && (viewport.request.reason == ImageViewport::RequestReason::UploadPending
            || viewport.request.reason == ImageViewport::RequestReason::RenderWaiting)
        && !viewport.itemBounds().isEmpty()) {
        if (viewport.hasProviderSequence()
            && !viewport.display.pendingRenderPayload.image.isNull()) {
            changes.scheduleUpdate = true;
            return changes;
        }
        viewport.publishSequenceReadyState();
        if (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
            setPlaybackPhase(viewport, changes,
                viewport.request.stopPlaybackWhenRequestReady
                    ? ImageViewport::PlaybackPhase::Stopped
                    : ImageViewport::PlaybackPhase::Playing);
            viewport.request.stopPlaybackWhenRequestReady = false;
        }
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
        changes.displayState = true;
    } else if (viewport.hasProviderSequence()
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && viewport.request.reason == ImageViewport::RequestReason::UploadPending
        && viewport.itemBounds().isEmpty()
        && !viewport.display.pendingRenderPayload.image.isNull()) {
        viewport.request.reason = ImageViewport::RequestReason::RenderWaiting;
        changes.requestRevision = true;
        changes.requestState = true;
        changes.displayRevision = true;
    } else {
        changes.displayRevision = true;
    }

    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
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
        = preparedPayload.requestId == 0 ? 0 : viewport.display.nextPreparedPayloadId + 1;
    return {
        viewport.provider.metadataReady,
        viewport.provider.timedMetadata,
        viewport.provider.logicalSize,
        viewport.provider.timingIntervals,
        viewport.request.activeRequest.target.frame,
        preparedPayload,
    };
}

ViewportProviderFrameEventAcceptance ViewportController::acceptProviderFrameEvent(
    ViewportProviderFrameEvent event) const
{
    if (!viewport.hasProviderSequence() || !viewport.provider.session
        || !activeProviderFrameTokenMatchesActiveRequest(viewport, event.token)) {
        return {};
    }

    return { true, providerFramePreparationState() };
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameEvent(
    ViewportProviderFrameEvent event, ImageFrame* frame,
    ImageSequenceProviderFrameMetadata metadata)
{
    const ViewportProviderFrameEventAcceptance frameEvent = acceptProviderFrameEvent(event);
    if (!frameEvent.accepted) {
        return {};
    }

    return handleProviderFrameAdmission(
        FramePreparation::admitProviderFrame(frame, metadata, frameEvent.preparationState));
}

ViewportProviderMetadataEventAcceptance ViewportController::acceptProviderMetadataEvent(
    ViewportProviderMetadataEvent event)
{
    if (!viewport.hasProviderSequence() || !viewport.provider.session
        || !viewport.provider.activeMetadataToken.isValid()
        || event.token != viewport.provider.activeMetadataToken) {
        return {};
    }

    viewport.provider.activeMetadataToken = {};
    return { true };
}

void ViewportController::handleProviderSessionOpenFailure(const QString& diagnostic)
{
    viewport.request.status = ImageViewport::RequestStatus::Error;
    viewport.request.reason = ImageViewport::RequestReason::ProviderFailure;
    viewport.request.errorString = diagnostic;
}

ViewportProviderSessionOpenResult ViewportController::handleProviderSessionOpened()
{
    ViewportProviderSessionOpenResult result;
    if (viewport.provider.metadataReady) {
        viewport.discardPendingRenderCommit();
        appendProviderFrameStartResult(result.providerFrameTransport,
            startProviderFrameRequest({ viewport.request.activeRequest.target }));
        return result;
    }

    appendProviderMetadataStartResult(
        result.providerMetadataTransport, startProviderMetadataRequest());
    return result;
}

quint64 ViewportController::installProviderSession(ImageSequenceProviderSession* session)
{
    viewport.provider.session = session;
    if (!viewport.provider.session) {
        return 0;
    }

    ++viewport.provider.sessionSerial;
    return viewport.provider.sessionSerial;
}

ImageSequenceProviderSession* ViewportController::takeProviderSession()
{
    ImageSequenceProviderSession* session = viewport.provider.session;
    viewport.provider.session.clear();
    return session;
}

ImageSequenceProviderSession* ViewportController::currentProviderSession() const
{
    return viewport.provider.session;
}

bool ViewportController::acceptsProviderSessionResult(quint64 sessionSerial) const
{
    return viewport.provider.session && viewport.provider.sessionSerial == sessionSerial;
}

ViewportProviderMetadataAdmissionResult ViewportController::handleProviderMetadataAdmission(
    const ImageSequenceProviderMetadata& metadata)
{
    const auto generationTerminalResult = [this](ImageViewportInternal::ViewportChangeSet changes) {
        ViewportProviderMetadataAdmissionResult result;
        result.changes = changes;
        result.providerFrameTransport.closeSession = viewport.provider.session != nullptr;
        result.providerFrameTransport.sessionClose = handleProviderSessionClose();
        return result;
    };

    const auto admission = FramePreparation::admitProviderMetadata(metadata);
    if (!admission.accepted()) {
        return generationTerminalResult(
            handleProviderMetadataAdmissionRejection({ admission.diagnostic }));
    }
    if (ImageViewportInternal::providerCapabilityContradictsMetadata(
            viewport.providerTimedPlaybackCapability(), metadata.timedPlaybackSupport())
        || ImageViewportInternal::providerCapabilityContradictsMetadata(
            viewport.providerFrameSeekCapability(), metadata.frameSeekSupport())
        || ImageViewportInternal::providerCapabilityContradictsMetadata(
            viewport.providerPositionSeekCapability(), metadata.positionSeekSupport())) {
        return generationTerminalResult(handleProviderMetadataContradiction(
            { QStringLiteral("provider metadata contradicts construction-time capabilities") }));
    }
    if (ImageViewportInternal::providerFactsContradictMetadata(
            viewport.providerKnownFacts(), metadata)) {
        return generationTerminalResult(handleProviderMetadataContradiction(
            { QStringLiteral("provider metadata contradicts construction-time facts") }));
    }

    ViewportProviderMetadataAdmissionResult result;
    result.accepted = true;
    result.facts = {
        admission.timedMetadata,
        metadata.timedPlaybackSupport(),
        metadata.frameSeekSupport(),
        metadata.positionSeekSupport(),
        admission.logicalSize,
        admission.timingIntervals,
    };
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameAdmission(
    const FramePreparation::ProviderFrameAdmissionResult& admission)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (!admission.accepted()) {
        clearQueuedProviderFrameRequest(viewport);
        viewport.provider.activeFrameToken = {};
        viewport.provider.activeFrameRequestId = 0;
        viewport.provider.activeFrameFromPlayback = false;
        viewport.provider.activeFrameTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
        viewport.request.status = admission.status;
        viewport.request.reason = admission.reason;
        viewport.request.errorString = admission.diagnostic;
        setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
        changes.requestRevision = true;
        changes.requestState = true;
        changes.diagnostics = true;
        return changes;
    }

    const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    viewport.publishAcceptedTargetState(admission.preparedPayload);
    if (viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewport.request.status == ImageViewport::RequestStatus::Ready
        && !viewport.display.pendingRenderPayload.commitPending) {
        setPlaybackPhase(viewport, changes,
            viewport.request.stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                          : ImageViewport::PlaybackPhase::Playing);
        viewport.request.stopPlaybackWhenRequestReady = false;
    }
    changes.requestRevision = true;
    changes.displayRevision = true;
    changes.requestState = true;
    changes.displayState = true;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.diagnostics = diagnosticsValueChanged;
    changes.scheduleUpdate = true;
    return changes;
}

ViewportProviderTerminalEventResult ViewportController::handleProviderTerminalEvent(
    const ViewportProviderTerminalEvent& event)
{
    if (!viewport.hasProviderSequence() || !viewport.provider.session) {
        return {};
    }

    if (activeProviderFrameTokenMatchesActiveRequest(viewport, event.token)) {
        return { handleProviderFrameTerminalResult(frameTerminalResultFor(event)), {} };
    }

    if (viewport.provider.metadataReady || !viewport.provider.activeMetadataToken.isValid()
        || event.token != viewport.provider.activeMetadataToken) {
        return {};
    }

    ViewportProviderTerminalEventResult result;
    result.changes = handleProviderMetadataTerminalResult(metadataTerminalResultFor(event));
    result.providerFrameTransport.closeSession = true;
    result.providerFrameTransport.sessionClose = handleProviderSessionClose();
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderFrameTerminalResult(
    const ViewportProviderFrameTerminalResult& result)
{
    ImageViewportInternal::ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(viewport);
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.request.status = result.status;
    viewport.request.reason = result.reason;
    viewport.request.errorString
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
    viewport.request.status = result.status;
    viewport.request.reason = result.reason;
    viewport.request.errorString
        = ImageViewportPrivate::boundedDiagnostic(result.diagnostic, result.fallbackDiagnostic);
    viewport.provider.activeMetadataToken = {};
    viewport.request.providerPlaybackStartPending = false;
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataContradiction(
    const ViewportProviderMetadataContradiction& contradiction)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewport.request.status = ImageViewport::RequestStatus::Error;
    viewport.request.reason = ImageViewport::RequestReason::PayloadRejection;
    viewport.request.errorString = contradiction.diagnostic;
    viewport.request.providerPlaybackStartPending = false;
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderMetadataAdmissionRejection(
    const ViewportProviderMetadataAdmissionRejection& rejection)
{
    ImageViewportInternal::ViewportChangeSet changes;
    viewport.request.status = ImageViewport::RequestStatus::Error;
    viewport.request.reason = ImageViewport::RequestReason::PayloadRejection;
    viewport.request.errorString = rejection.diagnostic;
    viewport.request.providerPlaybackStartPending = false;
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = true;
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderMetadataTargetRejection(
    ViewportProviderMetadataTargetRejection rejection)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (rejection.updateActiveTarget) {
        viewport.request.activeRequest.target.frame = rejection.selectedFrame;
        if (!rejection.selectedFromPosition) {
            viewport.request.activeRequest.target.position = -1;
        }
        viewport.request.playbackPosition = -1;
    }
    viewport.request.status = rejection.status;
    viewport.request.reason = rejection.reason;
    const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
    if (rejection.clearPlaybackStartPending) {
        viewport.request.providerPlaybackStartPending = false;
    }
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = diagnosticsValueChanged;
    return changes;
}

ViewportProviderMetadataTargetPolicyResult ViewportController::handleProviderMetadataTargetPolicy(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    const bool selectedFromPlaybackStart = viewport.request.providerPlaybackStartPending
        && viewport.request.activeRequest.target.providerTargetKind
            == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    const bool selectedFromPosition = viewport.request.activeRequest.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Position;
    ImageViewportInternal::ProviderRequestTargetKind requestTargetKind = selectedFromPlaybackStart
        ? ImageViewportInternal::ProviderRequestTargetKind::Playback
        : (selectedFromPosition ? ImageViewportInternal::ProviderRequestTargetKind::Position
                                : ImageViewportInternal::ProviderRequestTargetKind::Frame);
    int selectedFrame = viewport.request.activeRequest.target.frame >= 0
        ? viewport.request.activeRequest.target.frame
        : 0;
    const int providerFrameCount = facts.timedMetadata ? facts.timingIntervals.frameCount() : 1;
    if (selectedFromPlaybackStart
        && (!facts.timedMetadata || !viewport.provider.timedPlaybackSupport)) {
        return { handleProviderMetadataTargetRejection({ ImageViewport::RequestStatus::Unsupported,
            ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, true }) };
    }
    if (selectedFromPosition) {
        if (!facts.timedMetadata || !viewport.provider.positionSeekSupport) {
            return { handleProviderMetadataTargetRejection(
                { ImageViewport::RequestStatus::Unsupported,
                    ImageViewport::RequestReason::UnsupportedRequest, -1, false, false, false }) };
        }
        selectedFrame = viewport.providerFrameIndexForPosition(
            viewport.request.activeRequest.target.position);
    }
    if (selectedFrame < 0 || selectedFrame >= providerFrameCount) {
        return { handleProviderMetadataTargetRejection({ ImageViewport::RequestStatus::Unsupported,
            ImageViewport::RequestReason::InvalidRequest, selectedFrame, true, selectedFromPosition,
            false }) };
    }

    return handleProviderMetadataTargetSelection(
        { requestTargetKind, selectedFrame, selectedFromPosition, facts.timedMetadata });
}

ViewportProviderMetadataTargetPolicyResult
ViewportController::handleProviderMetadataTargetSelection(
    ViewportProviderMetadataTargetSelection selection)
{
    ViewportProviderMetadataTargetPolicyResult result;
    const bool rememberAsLatestNonPlayback
        = selection.targetKind != ImageViewportInternal::ProviderRequestTargetKind::Playback;
    const int selectedPosition = selection.selectedFromPosition
        ? viewport.request.activeRequest.target.position
        : selection.timedMetadata ? viewport.providerFrameStartPosition(selection.selectedFrame)
                                  : -1;
    beginAcceptedDisplayRequest(viewport,
        ImageViewportInternal::DisplayRequestOrigin::MetadataBoundSelection,
        { selection.selectedFrame, selectedPosition, selection.targetKind },
        rememberAsLatestNonPlayback);
    viewport.request.playbackPosition = viewport.request.activeRequest.target.position;
    viewport.publishProviderFrameLoadingState();

    viewport.request.providerPlaybackStartPending = false;
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest({ viewport.request.activeRequest.target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (!start.accepted) {
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = true;
        return result;
    }

    result.changes.requestRevision = true;
    result.changes.requestState = true;
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderAcceptedMetadataFacts(
    const ViewportProviderAcceptedMetadataFacts& facts)
{
    viewport.provider.metadataReady = true;
    viewport.provider.timedMetadata = facts.timedMetadata;
    viewport.provider.timedPlaybackSupport = facts.timedPlaybackSupport;
    viewport.provider.frameSeekSupport = facts.frameSeekSupport;
    viewport.provider.positionSeekSupport = facts.positionSeekSupport;
    viewport.provider.logicalSize = facts.logicalSize;
    viewport.provider.timingIntervals = facts.timingIntervals;
    return {};
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaitingEvent(
    ViewportProviderWaitingEvent event)
{
    if (!viewport.hasProviderSequence() || !viewport.provider.session) {
        return {};
    }
    if (event.progress
        && (!std::isfinite(event.progressValue) || event.progressValue < 0.0
            || event.progressValue > 1.0)) {
        return {};
    }

    const bool activeMetadataToken = !viewport.provider.metadataReady
        && viewport.provider.activeMetadataToken.isValid()
        && event.token == viewport.provider.activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(viewport, event.token);
    if (!activeMetadataToken && !activeFrameToken) {
        return {};
    }

    return handleProviderWaiting();
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleProviderWaiting()
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (viewport.request.status != ImageViewport::RequestStatus::Loading
        || viewport.request.reason == ImageViewport::RequestReason::ProviderWaiting) {
        return changes;
    }

    viewport.request.reason = ImageViewport::RequestReason::ProviderWaiting;
    changes.requestRevision = true;
    changes.requestState = true;
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderEndOfSequenceEvent(
    ViewportProviderEndOfSequenceEvent event)
{
    if (!viewport.hasProviderSequence() || !viewport.provider.session) {
        return {};
    }

    const bool activeMetadataToken = !viewport.provider.metadataReady
        && viewport.provider.activeMetadataToken.isValid()
        && event.token == viewport.provider.activeMetadataToken;
    const bool activeFrameToken
        = activeProviderFrameTokenMatchesActiveRequest(viewport, event.token);
    if (!activeMetadataToken && !activeFrameToken) {
        return {};
    }

    if (activeMetadataToken || !viewport.provider.metadataReady || !viewport.provider.timedMetadata
        || !viewport.provider.activeFrameFromPlayback) {
        ViewportProviderEndOfSequenceResult result;
        result.changes = handleProviderEndOfSequenceProtocolViolation(
            { activeMetadataToken, activeFrameToken });
        result.providerFrameTransport.closeSession = viewport.provider.session != nullptr;
        result.providerFrameTransport.sessionClose = handleProviderSessionClose();
        return result;
    }

    return handleProviderPlaybackEndOfSequence();
}

ImageViewportInternal::ViewportChangeSet
ViewportController::handleProviderEndOfSequenceProtocolViolation(
    ViewportProviderEndOfSequenceProtocolViolation violation)
{
    ImageViewportInternal::ViewportChangeSet changes;
    clearQueuedProviderFrameRequest(viewport);
    if (violation.activeMetadataToken) {
        viewport.provider.activeMetadataToken = {};
    }
    if (violation.activeFrameToken) {
        viewport.provider.activeFrameToken = {};
        viewport.provider.activeFrameRequestId = 0;
        viewport.provider.activeFrameFromPlayback = false;
        viewport.provider.activeFrameTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    }
    viewport.request.status = ImageViewport::RequestStatus::Error;
    viewport.request.reason = ImageViewport::RequestReason::PayloadRejection;
    viewport.request.errorString = QStringLiteral("provider protocol violation");
    viewport.request.providerPlaybackStartPending = false;
    viewport.request.stopPlaybackWhenRequestReady = false;
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);
    changes.requestRevision = true;
    changes.requestState = true;
    changes.diagnostics = true;
    return changes;
}

ViewportProviderEndOfSequenceResult ViewportController::handleProviderPlaybackEndOfSequence()
{
    ViewportProviderEndOfSequenceResult result;
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();

    int selectedFrame = 0;
    int selectedPosition = 0;
    if (viewport.request.looping) {
        viewport.request.stopPlaybackWhenRequestReady = false;
        viewport.request.playbackPosition = 0;
    } else {
        selectedFrame = viewport.frameCount() - 1;
        selectedPosition = viewport.providerFrameStartPosition(selectedFrame);
        viewport.request.playbackPosition = viewport.totalDuration();
        viewport.request.stopPlaybackWhenRequestReady = true;
    }

    viewport.request.activeRequest.target.frame = selectedFrame;
    viewport.request.activeRequest.target.position = selectedPosition;
    viewport.request.activeRequest.target.providerTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Playback;

    if (!viewport.request.looping && viewport.hasReadyDisplay()
        && viewport.display.displayedRequest.generation == viewport.request.sequenceGeneration
        && viewport.display.displayedRequest.request.target.frame == selectedFrame
        && viewport.display.displayedRequest.request.target.position == selectedPosition) {
        viewport.publishReadyDisplayState();
        setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Stopped);
        viewport.request.stopPlaybackWhenRequestReady = false;
        result.changes.requestRevision = true;
        result.changes.displayRevision = true;
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    viewport.publishProviderFrameLoadingState();
    const ViewportProviderFrameRequestStartResult start
        = startProviderFrameRequest({ viewport.request.activeRequest.target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (!start.accepted) {
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = true;
        return result;
    }
    setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Waiting);
    result.changes.requestRevision = true;
    result.changes.displayRevision = true;
    result.changes.requestState = true;
    result.changes.displayState = true;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportProviderFrameTransportEffect ViewportController::closeProviderSession()
{
    ViewportProviderFrameTransportEffect effect;
    effect.closeSession = viewport.provider.session != nullptr;
    effect.sessionClose = handleProviderSessionClose();
    return effect;
}

ViewportProviderSessionClose ViewportController::handleProviderSessionClose()
{
    ViewportProviderSessionClose sessionClose;
    clearQueuedProviderFrameRequest(viewport);
    if (!viewport.provider.session) {
        return sessionClose;
    }

    sessionClose.metadataToken = viewport.provider.activeMetadataToken;
    sessionClose.frameToken = viewport.provider.activeFrameToken;
    viewport.provider.activeMetadataToken = {};
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.provider.nextRequestToken = 0;
    return sessionClose;
}

ViewportProviderRequestTokenAllocation ViewportController::allocateProviderRequestToken()
{
    ViewportProviderRequestTokenAllocation allocation;
    if (viewport.provider.nextRequestToken == std::numeric_limits<quint64>::max()) {
        allocation.closeSession = viewport.provider.session != nullptr;
        allocation.sessionClose = handleProviderSessionClose();
        return allocation;
    }

    ++viewport.provider.nextRequestToken;
    allocation.token = ImageSequenceProviderRequestToken(viewport.provider.nextRequestToken);
    return allocation;
}

ViewportProviderMetadataRequestStartResult ViewportController::startProviderMetadataRequest()
{
    ViewportProviderMetadataRequestStartResult result;
    const ViewportProviderRequestTokenAllocation allocation = allocateProviderRequestToken();
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    viewport.provider.activeMetadataToken = allocation.token;
    if (!viewport.provider.activeMetadataToken.isValid()) {
        publishProviderTokenExhaustion(viewport);
        return result;
    }

    result.sendCommand = viewport.provider.session != nullptr;
    result.token = viewport.provider.activeMetadataToken;
    return result;
}

ViewportProviderFrameQueueResult ViewportController::queueProviderFrameRequest(
    ViewportProviderFrameQueueRequest request)
{
    ViewportProviderFrameQueueResult result;
    viewport.request.status = ImageViewport::RequestStatus::Loading;
    viewport.request.reason = ImageViewport::RequestReason::RequestQueued;
    viewport.display.status = viewport.display.displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    viewport.discardPendingRenderCommit();

    if (viewport.provider.session && viewport.provider.activeFrameToken.isValid()) {
        result.cancelToken = viewport.provider.activeFrameToken;
    }
    viewport.provider.activeFrameToken = {};
    viewport.provider.activeFrameRequestId = 0;
    viewport.provider.activeFrameFromPlayback = false;
    viewport.provider.activeFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;

    viewport.provider.queuedFrameRequest = true;
    viewport.provider.queuedFrameGeneration = viewport.request.sequenceGeneration;
    viewport.provider.queuedFrameRequestId = viewport.request.activeRequest.identity.id;
    viewport.provider.queuedFrame = request.frame;
    viewport.provider.queuedPosition = viewport.request.activeRequest.target.position;
    viewport.provider.queuedFrameFromPlayback
        = request.targetKind == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    viewport.provider.queuedFrameTargetKind = request.targetKind;
    result.scheduleFlush = true;
    return result;
}

ViewportProviderFrameQueueFlush ViewportController::flushQueuedProviderFrameRequest()
{
    ViewportProviderFrameQueueFlush flush;
    if (!viewport.provider.queuedFrameRequest || !viewport.hasProviderSequence()
        || !viewport.provider.session) {
        clearQueuedProviderFrameRequest(viewport);
        return flush;
    }

    const int queuedFrame = viewport.provider.queuedFrame;
    const int queuedPosition = viewport.provider.queuedPosition;
    const quint64 queuedRequestId = viewport.provider.queuedFrameRequestId;
    const ImageViewportInternal::ProviderRequestTargetKind queuedTargetKind
        = viewport.provider.queuedFrameTargetKind;
    const bool stillCurrent
        = viewport.provider.queuedFrameGeneration == viewport.request.sequenceGeneration
        && queuedRequestId == viewport.request.activeRequest.identity.id
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && viewport.request.reason == ImageViewport::RequestReason::RequestQueued
        && viewport.request.activeRequest.target.frame == queuedFrame
        && viewport.request.activeRequest.target.position == queuedPosition
        && viewport.request.activeRequest.target.providerTargetKind == queuedTargetKind;
    clearQueuedProviderFrameRequest(viewport);
    if (!stillCurrent) {
        return flush;
    }

    flush.startRequest = true;
    flush.frame = queuedFrame;
    flush.targetKind = queuedTargetKind;
    return flush;
}

ViewportProviderFrameRequestStartResult ViewportController::startProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameRequestStartResult result;
    clearQueuedProviderFrameRequest(viewport);
    viewport.request.status = ImageViewport::RequestStatus::Loading;
    viewport.request.reason = ImageViewport::RequestReason::ProviderWaiting;
    const ViewportProviderRequestTokenAllocation allocation = allocateProviderRequestToken();
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    viewport.provider.activeFrameToken = allocation.token;
    viewport.provider.activeFrameRequestId = viewport.request.activeRequest.identity.id;
    if (!viewport.provider.activeFrameToken.isValid()) {
        publishProviderTokenExhaustion(viewport);
        return result;
    }

    viewport.request.activeRequest.providerFrameToken = viewport.provider.activeFrameToken;
    viewport.provider.activeFrameTargetKind = request.target.providerTargetKind;
    viewport.provider.activeFrameFromPlayback = request.target.providerTargetKind
        == ImageViewportInternal::ProviderRequestTargetKind::Playback;
    result.accepted = true;
    result.sendCommand = viewport.provider.session != nullptr;
    result.command.token = viewport.provider.activeFrameToken;
    result.command.frame = request.target.frame;
    result.command.position = request.target.position;
    result.command.targetKind = request.target.providerTargetKind;
    return result;
}

ViewportProviderFrameDispatchResult ViewportController::dispatchProviderFrameRequest(
    ViewportProviderFrameRequestStart request)
{
    ViewportProviderFrameDispatchResult result;
    if (viewport.provider.activeFrameToken.isValid()) {
        result.accepted = true;
        appendProviderFrameQueueResult(result.transport,
            queueProviderFrameRequest({ request.target.frame, request.target.providerTargetKind }));
        return result;
    }

    const ViewportProviderFrameRequestStartResult start = startProviderFrameRequest(request);
    result.accepted = start.accepted;
    appendProviderFrameStartResult(result.transport, start);
    return result;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization()
{
    ViewportRenderSynchronization synchronization;
    synchronization.pendingProviderCommit = viewport.hasProviderSequence()
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && (viewport.request.reason == ImageViewport::RequestReason::UploadPending
            || viewport.request.reason == ImageViewport::RequestReason::RenderWaiting)
        && viewport.display.pendingRenderPayload.commitPending
        && !viewport.display.pendingRenderPayload.image.isNull()
        && !viewport.itemBounds().isEmpty();
    synchronization.oldContentRect = viewport.contentRect();
    synchronization.oldVisibleImageRect = viewport.visibleImageRect();
    synchronization.oldDisplayStatus = viewport.display.status;
    if (synchronization.pendingProviderCommit) {
        synchronization.preparedPayload = viewport.display.pendingRenderPayload;
    } else if (viewport.display.pendingRenderPayload.commitPending && viewport.hasReadyDisplay()) {
        synchronization.preparedPayload = viewport.display.pendingRenderPayload;
        synchronization.preparedPayload.image = viewport.display.displayedImage;
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

    const bool renderMatchesPending
        = renderAcknowledgementMatchesPending(viewport, acknowledgement);
    if (renderMatchesPending && synchronization.pendingProviderCommit) {
        viewport.publishSequenceReadyState(synchronization.preparedPayload);
    }
    const bool resumePlaybackAfterCommit = renderMatchesPending
        && viewport.request.playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewport.request.status == ImageViewport::RequestStatus::Ready;
    if (renderMatchesPending) {
        viewport.display.commitDisplayedRequestSnapshot(viewport.request.sequenceGeneration,
            viewport.request.activeRequest, viewport.display.pendingRenderPayload.payloadId);
        viewport.display.clearPendingRenderPayload();
    }
    viewport.display.clearRenderFailureRetainedDisplay();
    if (resumePlaybackAfterCommit) {
        setPlaybackPhase(viewport, changes,
            viewport.request.stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                          : ImageViewport::PlaybackPhase::Playing);
        viewport.request.stopPlaybackWhenRequestReady = false;
    }
    if (synchronization.pendingProviderCommit) {
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
        changes.displayState = viewport.display.status != synchronization.oldDisplayStatus;
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
    const bool renderMatchesPending
        = renderAcknowledgementMatchesPending(viewport, acknowledgement);
    const bool pendingProviderCommit = viewport.hasProviderSequence()
        && viewport.request.status == ImageViewport::RequestStatus::Loading
        && (viewport.request.reason == ImageViewport::RequestReason::UploadPending
            || viewport.request.reason == ImageViewport::RequestReason::RenderWaiting)
        && viewport.display.pendingRenderPayload.commitPending
        && !viewport.display.pendingRenderPayload.image.isNull();
    if (!renderMatchesPending
        || (viewport.display.status != ImageViewport::DisplayStatus::Ready
            && !pendingProviderCommit)) {
        return changes;
    }

    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewport.display.status;

    viewport.request.status = ImageViewport::RequestStatus::Error;
    viewport.request.reason = ImageViewport::RequestReason::RenderFailure;
    viewport.display.clearPendingRenderPayload();
    if (viewport.display.renderFailureRetainedDisplayValid) {
        viewport.display.status = ImageViewport::DisplayStatus::Retained;
        viewport.display.displayedRequest = viewport.display.renderFailureRetainedRequest;
        viewport.display.displayedImageSize = viewport.display.renderFailureRetainedImageSize;
        viewport.display.displayedImage = viewport.display.renderFailureRetainedImage;
    } else {
        viewport.display.status = ImageViewport::DisplayStatus::Empty;
        viewport.display.clearDisplayedDisplay();
    }
    viewport.display.clearRenderFailureRetainedDisplay();
    viewport.request.errorString = QStringLiteral("render commit failed");
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);

    changes.requestRevision = true;
    changes.displayRevision = true;
    changes.requestState = true;
    changes.displayState = viewport.display.status != oldDisplayStatus;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    changes.diagnostics = true;
    return changes;
}

ViewportPlaybackAdvanceResult ViewportController::advancePlayback(int elapsedMilliseconds)
{
    ViewportPlaybackAdvanceResult result;
    if (viewport.request.playbackPhase != ImageViewport::PlaybackPhase::Playing
        || elapsedMilliseconds <= 0) {
        return result;
    }

    if (viewport.hasProviderSequence() && viewport.provider.metadataReady
        && viewport.provider.timedMetadata) {
        const int duration = viewport.totalDuration();
        const int previousFrame = viewport.request.activeRequest.target.frame;
        const int currentFrame = viewport.request.activeRequest.target.frame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, currentFrame, viewport.request.playbackPosition,
            viewport.request.looping, duration, viewport.frameCount(),
            [this](int frame) { return viewport.providerFrameStartPosition(frame); },
            [this](int position) { return viewport.providerFrameIndexForPosition(position); });
        if (!target.valid) {
            return result;
        }

        viewport.request.playbackPosition = target.playbackPosition;
        if (target.displayTarget.frame == previousFrame
            && viewport.request.status == ImageViewport::RequestStatus::Ready) {
            if (viewport.request.stopPlaybackWhenRequestReady) {
                setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Stopped);
                viewport.request.stopPlaybackWhenRequestReady = false;
            } else {
                applyPlaybackAdvancePhase(viewport, result.changes, target);
            }
            return result;
        }

        applyPlaybackTarget(viewport, target.displayTarget);
        viewport.request.activeRequest.target.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Playback;
        viewport.publishProviderFrameLoadingState();
        const bool diagnosticsValueChanged = viewport.request.clearDiagnostics();
        const ViewportProviderFrameDispatchResult dispatch
            = dispatchProviderFrameRequest({ viewport.request.activeRequest.target });
        result.providerFrameTransport = dispatch.transport;
        appendPlaybackRequestChange(viewport, result.changes, previousFrame);
        if (!dispatch.accepted) {
            result.changes.diagnostics = true;
            result.changes.scheduleUpdate = true;
            return result;
        }

        applyPlaybackAdvancePhase(viewport, result.changes, target);
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    if (!viewport.hasTimedSequence()) {
        return result;
    }

    const int totalDuration = viewport.totalDuration();
    const int previousFrame = viewport.request.activeRequest.target.frame;
    const int currentFrame = viewport.request.activeRequest.target.frame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, currentFrame, viewport.request.playbackPosition,
        viewport.request.looping, totalDuration, viewport.sequenceFrameCount(),
        [this](int frame) { return viewport.sequenceFrameStartPosition(frame); },
        [this](int position) { return viewport.sequenceFrameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    viewport.request.playbackPosition = target.playbackPosition;
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        return result;
    }

    applyPlaybackTarget(viewport, target.displayTarget);
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    viewport.publishAcceptedTargetState();
    applyPlaybackAdvancePhase(viewport, result.changes, target);
    appendPlaybackRequestChange(viewport, result.changes, previousFrame);
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.scheduleUpdate = true;
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

quint64 ViewportController::pendingRenderGenerationForTest() const
{
    return viewport.pendingRenderGenerationForTestImpl();
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
        ++request.commandRevision;
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
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.play();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.pause();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.stop();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seek(frame);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seekToPosition(milliseconds);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    const ViewportCommandResult result = controller.resetView();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
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

quint64 ImageViewportPrivate::pendingRenderGenerationForTest() const
{
    return controller.pendingRenderGenerationForTest();
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTest() const
{
    return controller.pendingRenderPayloadIdForTest();
}

void ImageViewportPrivate::acknowledgeRenderCommitForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const auto changes = controller.acknowledgeRenderCommit(
        { { generation, requestId, preparedPayloadId } }, true, synchronization);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::acknowledgeRenderFailureForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    const auto changes
        = controller.acknowledgeRenderFailure({ { generation, requestId, preparedPayloadId } });
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}
#endif
