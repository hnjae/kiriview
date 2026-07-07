#include "viewportcontrollerhelpers_p.h"
#include "viewportcommandoutcome_p.h"

namespace {
void clearQueuedProviderFrameRequest(ViewportControllerPort& viewport)
{
    ImageViewportInternal::ProviderGenerationState& provider = viewportProviderState(viewport);
    provider.queuedFrameRequest = false;
    provider.queuedFrameGeneration = 0;
    provider.queuedFrameRequestId = 0;
    provider.queuedFrame = -1;
    provider.queuedPosition = -1;
    provider.queuedResolvedFrame = {};
    provider.queuedFrameFromPlayback = false;
    provider.queuedFrameTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
}

DisplayRequestTarget providerLatestNonPlaybackTarget(ViewportControllerPort& viewport)
{
    DisplayRequestTarget target;
    target.frame = viewportRequestState(viewport).latestNonPlaybackRequest.target.frame;
    target.position = viewportRequestState(viewport).latestNonPlaybackRequest.target.position;
    target.providerTargetKind
        = viewportRequestState(viewport).latestNonPlaybackRequest.target.providerTargetKind;
    return target;
}

DisplayRequestTarget providerStopRestoreTarget(ViewportControllerPort& viewport)
{
    DisplayRequestTarget target = providerLatestNonPlaybackTarget(viewport);
    if (target.frame < 0 && target.position >= 0) {
        target.frame = viewport.providerFrameIndexForPosition(target.position);
    }
    if (target.frame < 0 && target.position < 0
        && viewportRequestState(viewport).activeRequest.target.frame >= 0) {
        target.frame = viewportRequestState(viewport).activeRequest.target.frame;
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

ImageViewportInternal::ResolvedFrameIdentity providerStopRestoreResolvedFrame(
    ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    if (viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame.isValid()
        && viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame.frame
            == target.frame) {
        return viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame;
    }
    if (target.frame < 0) {
        return {};
    }
    return { target.frame, viewport.providerFrameStartPosition(target.frame) };
}

void beginStopRestoreDisplayRequest(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    viewportRequestState(viewport).beginDisplayRequest(
        ImageViewportInternal::DisplayRequestOrigin::StopRestore, target, resolvedFrame, true);
    viewportRequestState(viewport).playbackPosition = target.position;
}

void applyStopRestoreTarget(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    beginStopRestoreDisplayRequest(viewport, target, resolvedFrame);
}

void applyProviderStopRestoreTarget(ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    beginStopRestoreDisplayRequest(viewport, target, resolvedFrame);
}

bool stopRestoreTargetIsReadyDisplay(ViewportControllerPort& viewport)
{
    return viewport.hasReadyDisplay()
        && viewportDisplayState(viewport).displayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.frame
        == viewportRequestState(viewport).activeRequest.resolvedFrame.frame
        && viewportDisplayState(viewport).displayedRequest.request.resolvedFrame.position
        == viewportRequestState(viewport).activeRequest.resolvedFrame.position;
}

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
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame;
};

StopRestorePlan stopRestorePlanFor(ViewportControllerPort& viewport)
{
    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewportRequestState(viewport).activeRequest.target.frame < 0
        && viewportRequestState(viewport).activeRequest.target.position < 0) {
        return { StopRestorePlanKind::ProviderPendingMetadata,
            providerLatestNonPlaybackTarget(viewport),
            viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame };
    }
    if (viewport.hasProviderSequence() && viewportProviderState(viewport).timedMetadata
        && viewportProviderState(viewport).queuedFrameRequest
        && viewportProviderState(viewport).queuedFrameFromPlayback) {
        const DisplayRequestTarget target = providerStopRestoreTarget(viewport);
        return { StopRestorePlanKind::ProviderQueuedPlayback, target,
            providerStopRestoreResolvedFrame(viewport, target) };
    }
    if (viewport.hasProviderSequence() && viewportProviderState(viewport).timedMetadata
        && activeProviderFrameRequestIsPlayback(viewport)) {
        const DisplayRequestTarget target = providerStopRestoreTarget(viewport);
        return { StopRestorePlanKind::ProviderActivePlayback, target,
            providerStopRestoreResolvedFrame(viewport, target) };
    }
    if (viewport.hasTimedSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting
        && (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && viewportRequestState(viewport).latestNonPlaybackRequest.target.frame >= 0
        && viewportRequestState(viewport).activeRequest.target.frame
            != viewportRequestState(viewport).latestNonPlaybackRequest.target.frame) {
        return { StopRestorePlanKind::BuiltInRenderWait,
            DisplayRequestTarget {
                viewportRequestState(viewport).latestNonPlaybackRequest.target.frame,
                viewportRequestState(viewport).latestNonPlaybackRequest.target.position },
            viewportRequestState(viewport).latestNonPlaybackRequest.resolvedFrame };
    }
    return {};
}

StopRestorePublication publishStopRestoreTarget(ViewportController& controller,
    ViewportControllerPort& viewport,
    DisplayRequestTarget target, ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
    StopRestoreWaitingState waitingState)
{
    StopRestorePublication publication;
    publication.oldDisplayStatus = viewportDisplayState(viewport).status;
    if (waitingState == StopRestoreWaitingState::ProviderLoading) {
        applyProviderStopRestoreTarget(viewport, target, resolvedFrame);
    } else {
        applyStopRestoreTarget(viewport, target, resolvedFrame);
    }

    publication.readyDisplay = stopRestoreTargetIsReadyDisplay(viewport);
    if (publication.readyDisplay) {
        controller.publishReadyDisplayState();
    } else if (waitingState == StopRestoreWaitingState::ProviderLoading) {
        controller.publishProviderFrameLoadingState();
    } else {
        controller.publishRenderWaitingState();
    }
    return publication;
}

void completeStopRestoreRequest(
    ViewportController& controller, ViewportControllerPort& viewport, ViewportCommandResult& result)
{
    controller.setPlaybackPhase(result, ImageViewport::PlaybackPhase::Stopped);
    result.changes.requestRevision = true;
    result.changes.requestState = true;
}

bool appendProviderStopRestoreFrameStart(
    ViewportController& controller, ViewportControllerPort& viewport, ViewportCommandResult& result)
{
    if (!viewportProviderState(viewport).session
        || viewportRequestState(viewport).activeRequest.target.frame < 0) {
        return true;
    }

    DisplayRequestTarget target = viewportRequestState(viewport).activeRequest.target;
    target.providerTargetKind
        = viewportRequestState(viewport).latestNonPlaybackRequest.target.providerTargetKind;
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

bool applyStopRestorePlan(ViewportController& controller, ViewportControllerPort& viewport,
    StopRestorePlan plan, ViewportCommandResult& result)
{
    switch (plan.kind) {
    case StopRestorePlanKind::ProviderPendingMetadata:
        applyProviderStopRestoreTarget(viewport, plan.target, plan.resolvedFrame);
        viewportRequestState(viewport).providerPlaybackStartPending = false;
        completeStopRestoreRequest(controller, viewport, result);
        return true;
    case StopRestorePlanKind::ProviderQueuedPlayback: {
        clearQueuedProviderFrameRequest(viewport);

        const StopRestorePublication publication = publishStopRestoreTarget(controller,
            viewport, plan.target, plan.resolvedFrame, StopRestoreWaitingState::ProviderLoading);
        if (!publication.readyDisplay) {
            if (!appendProviderStopRestoreFrameStart(controller, viewport, result)) {
                return true;
            }
        }
        completeStopRestoreRequest(controller, viewport, result);
        return true;
    }
    case StopRestorePlanKind::ProviderActivePlayback: {
        if (viewportProviderState(viewport).session) {
            result.providerFrameTransport.cancelToken
                = viewportProviderState(viewport).activeFrameToken;
        }
        viewportProviderState(viewport).activeFrameToken = {};

        const StopRestorePublication publication = publishStopRestoreTarget(controller,
            viewport, plan.target, plan.resolvedFrame, StopRestoreWaitingState::ProviderLoading);
        if (publication.readyDisplay) {
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            completeStopRestoreRequest(controller, viewport, result);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return true;
        }
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        if (!appendProviderStopRestoreFrameStart(controller, viewport, result)) {
            return true;
        }
        completeStopRestoreRequest(controller, viewport, result);
        result.changes.diagnostics = diagnosticsValueChanged;
        return true;
    }
    case StopRestorePlanKind::BuiltInRenderWait: {
        const StopRestorePublication publication = publishStopRestoreTarget(controller,
            viewport, plan.target, plan.resolvedFrame, StopRestoreWaitingState::RenderWaiting);
        completeStopRestoreRequest(controller, viewport, result);
        result.changes.displayRevision
            = viewportDisplayState(viewport).status != publication.oldDisplayStatus;
        result.changes.displayState = result.changes.displayRevision;
        result.changes.scheduleUpdate = true;
        return true;
    }
    case StopRestorePlanKind::None:
        return false;
    }
    return false;
}

enum class ExplicitSeekMaterialization {
    ProviderReady,
    ProviderPendingMetadata,
    BuiltIn,
};

ViewportCommandResult acceptExplicitSeek(ViewportController& controller,
    ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
    ExplicitSeekMaterialization materialization)
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
    controller.beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        target, resolvedFrame, true);
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).playbackPosition = target.position;

    switch (materialization) {
    case ExplicitSeekMaterialization::ProviderReady: {
        controller.publishProviderFrameLoadingState();
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
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
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing) {
            controller.setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
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
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        controller.discardPendingRenderCommit();
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = diagnosticsValueChanged;
        return result;
    }
    case ExplicitSeekMaterialization::BuiltIn: {
        const QRectF oldContentRect = viewport.contentRect();
        const QRectF oldVisibleImageRect = viewport.visibleImageRect();
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        controller.publishAcceptedTargetState();
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
            controller.setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
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

ViewportCommandResult acceptExplicitSeek(ViewportController& controller,
    ViewportControllerPort& viewport, DisplayRequestTarget target,
    ExplicitSeekMaterialization materialization)
{
    return acceptExplicitSeek(
        controller, viewport, target, { target.frame, target.position }, materialization);
}

void applyProviderPlaybackStartTarget(ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    viewportRequestState(viewport).providerPlaybackStartPending = false;
    viewportRequestState(viewport).activeRequest.target.frame = target.frame;
    viewportRequestState(viewport).activeRequest.target.position = target.position;
    viewportRequestState(viewport).activeRequest.resolvedFrame
        = { target.frame, viewport.providerFrameStartPosition(target.frame) };
    viewportRequestState(viewport).playbackPosition = target.position;
    viewportRequestState(viewport).activeRequest.target.providerTargetKind
        = target.providerTargetKind;
}

void applyPendingProviderPlaybackTarget(
    ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    applyPendingProviderPlaybackTargetForRole(
        viewport, ImageViewport::PageRole::Primary, target);
}

void beginPlaybackDisplayRequestForRole(ViewportController& controller,
    ViewportControllerPort& viewport, ImageViewport::PageRole role,
    ImageViewportInternal::DisplayRequestOrigin origin, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame, bool rememberAsLatestNonPlayback)
{
    if (role == ImageViewport::PageRole::Primary) {
        controller.beginAcceptedDisplayRequest(
            origin, target, resolvedFrame, rememberAsLatestNonPlayback);
        return;
    }

    const ImageViewportInternal::DisplayRequest primaryRequest
        = activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary);
    controller.beginAcceptedDisplayRequest(
        origin, primaryRequest.target, primaryRequest.resolvedFrame, false);
    controller.setSecondaryActiveRequest(target, resolvedFrame, rememberAsLatestNonPlayback);
}

template <typename FrameStartFor>
int playbackStartPosition(ViewportControllerPort& viewport, FrameStartFor frameStartFor)
{
    const auto& target = viewportRequestState(viewport).activeRequest.target;
    return target.position >= 0 ? target.position : frameStartFor(target.frame);
}

template <typename FrameStartFor>
void seedPlaybackPosition(ViewportControllerPort& viewport, FrameStartFor frameStartFor)
{
    viewportRequestState(viewport).playbackPosition
        = playbackStartPosition(viewport, frameStartFor);
}

void applyPlaybackTarget(ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    viewportRequestState(viewport).beginDisplayRequest(
        ImageViewportInternal::DisplayRequestOrigin::Playback, target, false);
}

void applyPlaybackAdvancePhase(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, const PlaybackAdvanceTarget& target)
{
    if (target.reachedEnd) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady
            = viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading;
    }
    const ImageViewport::PlaybackPhase phase = playbackAdvancePhaseForRequest(
        viewportRequestState(viewport).status, target.reachedEnd);
    if (viewportRequestState(viewport).playbackPhase == phase) {
        return;
    }

    viewportRequestState(viewport).playbackPhase = phase;
    changes.playbackPhase = true;
}

void appendPlaybackRequestChange(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, ImageViewport::PageRole role,
    int previousFrame)
{
    changes.requestRevision = true;
    if (activeRequestForRole(viewportRequestState(viewport), role).target.frame != previousFrame
        || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
        changes.displayRevision = true;
    }
    changes.requestState = true;
    changes.displayState = true;
}

void publishProviderPlaybackAdvanceLoadingState(ViewportController& controller,
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Primary) {
        controller.publishProviderFrameLoadingState();
        return;
    }

    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
    viewportDisplayState(viewport).status
        = viewportDisplayState(viewport).displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
}

ViewportCommandResult playBuiltInRole(ViewportController& controller,
    ViewportControllerPort& viewport, const ViewportControllerState& state,
    ImageViewport::PageRole role, bool generationTerminalFailure)
{
    Q_UNUSED(state);
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), role);
    if (!viewport.hasActiveRequest() || request.target.frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (generationTerminalFailure) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }
    if (!hasTimedBuiltInSequenceForRole(viewport, role)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
    viewportRequestState(viewport).playbackRole = role;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    viewportRequestState(viewport).playbackPosition
        = request.target.position >= 0 ? request.target.position : request.resolvedFrame.position;
    controller.setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
    return result;
}

ViewportCommandResult playProviderRole(ViewportController& controller,
    ViewportControllerPort& viewport, ViewportControllerState& state, ImageViewport::PageRole role,
    bool generationTerminalFailure)
{
    const ImageViewportInternal::DisplayRequest& request
        = activeRequestForRole(viewportRequestState(viewport), role);
    const bool currentIdentity = request.identity.id != 0
        && request.identity.id == viewportRequestState(viewport).activeRequest.identity.id;
    if (!viewport.hasActiveRequest() || !currentIdentity) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (generationTerminalFailure) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto provider = providerRoleStateFor(state, role);
    if (!provider.provider.metadataReady) {
        if (ImageViewportInternal::providerCapabilityKnownFalse(
                providerTimedPlaybackCapabilityForRole(viewport, role))) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        viewportRequestState(viewport).playbackRole = role;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        applyPendingProviderPlaybackTargetForRole(viewport, role, pendingProviderPlaybackTarget());
        controller.setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (request.target.frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (!provider.provider.timedMetadata || !provider.provider.timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
    viewportRequestState(viewport).playbackRole = role;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    viewportRequestState(viewport).playbackPosition
        = request.target.position >= 0 ? request.target.position : request.resolvedFrame.position;
    controller.setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
    return result;
}
}

void ViewportController::setPlaybackPhase(
    ViewportCommandResult& result, ImageViewport::PlaybackPhase phase)
{
    if (viewportRequestState(viewport).playbackPhase == phase) {
        return;
    }

    viewportRequestState(viewport).playbackPhase = phase;
    result.changes.playbackPhase = true;
}

void ViewportController::setPlaybackPhase(
    ImageViewportInternal::ViewportChangeSet& changes, ImageViewport::PlaybackPhase phase)
{
    if (viewportRequestState(viewport).playbackPhase == phase) {
        return;
    }

    viewportRequestState(viewport).playbackPhase = phase;
    changes.playbackPhase = true;
}

void ViewportController::armAuthoredAutoplayIfEligible()
{
    if (viewport.hasProviderSequence()) {
        const ImageSequenceAuthoredAnimationFacts facts = viewport.providerAuthoredAnimationFacts();
        if (!facts.autoplay() || viewport.providerTimedPlaybackCapabilityKnownFalse()) {
            return;
        }

        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        if (!viewportProviderState(viewport).metadataReady) {
            applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
            viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Waiting;
            return;
        }
        if (viewportProviderState(viewport).timedMetadata
            && viewportProviderState(viewport).timedPlaybackSupport) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.providerFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackPhase = playbackPhaseForCurrentRequest(viewport);
        }
        return;
    }

    const ImageSequenceAuthoredAnimationFacts facts = viewport.sequenceAuthoredAnimationFacts();
    if (!viewport.hasTimedSequence() || !facts.autoplay()) {
        return;
    }

    viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    seedPlaybackPosition(
        viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
    viewportRequestState(viewport).playbackPhase = playbackPhaseForCurrentRequest(viewport);
}

ViewportCommandResult ViewportController::play()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata
        && viewportProviderState(viewport).timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(viewportRequestState(viewport).playbackPhase,
                  viewportRequestState(viewport).stopPlaybackWhenRequestReady)
            && viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Primary
            && viewportRequestState(viewport).playbackPosition >= 0;
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Unsupported
            || viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error) {
            const DisplayRequestTarget target = providerPlaybackStartTarget(viewport);
            ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            applyProviderPlaybackStartTarget(viewport, target);
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            publishProviderFrameLoadingState();
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest({ target });
            result.providerFrameTransport = dispatch.transport;
            if (!dispatch.accepted) {
                result.changes.requestRevision = true;
                result.changes.requestState = true;
                result.changes.diagnostics = true;
                return result;
            }
            setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
            result.changes.requestRevision = true;
            result.changes.requestState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return result;
        }

        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.providerFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerTimedPlaybackCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
        setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (viewport.hasTimedSequence()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(viewportRequestState(viewport).playbackPhase,
                  viewportRequestState(viewport).stopPlaybackWhenRequestReady)
            && viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Primary
            && viewportRequestState(viewport).playbackPosition >= 0;
        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Unsupported
            || viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus
                = viewportDisplayState(viewport).status;
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            publishAcceptedTargetState();
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
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
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    ImageViewportInternal::CommandOutcome::markRejected(
        viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::play(ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (role == ImageViewport::PageRole::Primary) {
        return play();
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    const bool generationTerminalFailure = hasGenerationTerminalProviderFailure();
    if (hasProviderSequenceForRole(viewport, role)) {
        return commandResultWithRoleTransport(
            role, playProviderRole(*this, viewport, state, role, generationTerminalFailure));
    }
    if (!hasTimedBuiltInSequenceForRole(viewport, role)) {
        return rejectUnsupportedCommand();
    }
    return playBuiltInRole(*this, viewport, state, role, generationTerminalFailure);
}

ViewportCommandResult ViewportController::pause()
{
    return pause(ImageViewport::PageRole::Primary);
}

ViewportCommandResult ViewportController::pause(ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (role == ImageViewport::PageRole::Secondary && !hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped
        && viewportRequestState(viewport).playbackRole != role) {
        return result;
    }
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportCommandResult ViewportController::stop() { return stop(ImageViewport::PageRole::Primary); }

ViewportCommandResult ViewportController::stop(ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (role == ImageViewport::PageRole::Secondary && !hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped
        && viewportRequestState(viewport).playbackRole != role) {
        return result;
    }

    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    if (role == ImageViewport::PageRole::Secondary) {
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Stopped) {
            return result;
        }

        if (activeProviderFrameRequestIsPlayback(
                state, viewport, ImageViewport::PageRole::Secondary)) {
            const auto provider = providerRoleStateFor(state, role);
            result.secondaryProviderFrameTransport.cancelToken
                = provider.provider.activeFrameToken;
            provider.provider.activeFrameToken = {};
        }

        const ImageViewportInternal::DisplayRequest restoreRequest
            = latestNonPlaybackRequestForRole(viewportRequestState(viewport), role);
        if (restoreRequest.target.frame >= 0) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus
                = viewportDisplayState(viewport).status;
            beginPlaybackDisplayRequestForRole(*this, viewport, role,
                ImageViewportInternal::DisplayRequestOrigin::StopRestore, restoreRequest.target,
                restoreRequest.resolvedFrame, true);
            viewportRequestState(viewport).playbackPosition = restoreRequest.target.position;
            if (displayedPrimaryPayloadMatchesActiveTarget(viewport)
                && displayedSecondaryPayloadMatchesActiveTarget(viewport)) {
                publishReadyDisplayState();
            } else {
                publishAcceptedTargetState();
            }
            setPlaybackPhase(result, ImageViewport::PlaybackPhase::Stopped);
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.scheduleUpdate = true;
            return result;
        }

        setPlaybackPhase(result, ImageViewport::PlaybackPhase::Stopped);
        return result;
    }

    const StopRestorePlan stopRestorePlan = stopRestorePlanFor(viewport);
    if (applyStopRestorePlan(*this, viewport, stopRestorePlan, result)) {
        return result;
    }
    setPlaybackPhase(result, ImageViewport::PlaybackPhase::Stopped);
    return result;
}

ViewportCommandResult ViewportController::seek(int frame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasDisplayableSequence()) {
        if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady) {
            if (!viewportProviderState(viewport).frameSeekSupport) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            const int maximumFrame = viewportProviderState(viewport).timedMetadata
                ? viewportProviderState(viewport).timingIntervals.frameCount() - 1
                : 0;
            if (frame > maximumFrame) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget { frame, viewport.providerFrameStartPosition(frame),
                    ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderReady);
        }

        if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
            if (viewport.providerFrameSeekCapabilityKnownFalse()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (viewport.providerKnownFactsTimedFrameCount()
                && viewport.providerFrameSeekCapabilityKnownTrue()) {
                const int maximumFrame = viewport.providerKnownFactsFrameCount() - 1;
                if (frame > maximumFrame) {
                    ViewportCommandResult result;
                    result.outcome = ImageViewport::CommandOutcome::Invalid;
                    ImageViewportInternal::CommandOutcome::markRejected(
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
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget { frame,
                viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(frame) : -1 },
            ExplicitSeekMaterialization::BuiltIn);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    ImageViewportInternal::CommandOutcome::markRejected(
        viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seek(ImageViewport::PageRole role, int frame)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (role == ImageViewport::PageRole::Primary) {
        return seek(frame);
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (viewportRequestState(viewport).secondarySequenceIsProvider) {
        return commandResultWithSecondaryTransport(seekSecondaryProvider(frame));
    }
    if (frame < 0) {
        return rejectInvalidCommand();
    }

    const int frameCount = state.secondarySource.frameCount;
    if (frameCount <= 0 || frame >= frameCount) {
        return rejectInvalidCommand();
    }

    const int position = frameStartPositionForRoleSource(viewport, state.secondarySource, frame);
    return seekSecondaryBuiltIn(
        { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
        { frame, position });
}

ViewportCommandResult ViewportController::seekSecondaryBuiltIn(
    ImageViewportInternal::DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    const ImageViewportInternal::DisplayRequest primaryRequest
        = viewportRequestState(viewport).activeRequest;
    beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        primaryRequest.target, primaryRequest.resolvedFrame, true);
    setSecondaryActiveRequest(target, resolvedFrame, true);
    publishAcceptedTargetState();
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
    }

    result.changes.requestRevision = true;
    const bool displayValueChanged = viewportDisplayState(viewport).status != oldDisplayStatus
        || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
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

ViewportCommandResult ViewportController::seekSecondaryProvider(int frame)
{
    if (!viewport.hasActiveRequest() || !hasSecondaryProviderSequence(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto acceptTarget = [this](ImageViewportInternal::DisplayRequestTarget target,
                                  ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
                                  bool dispatchNow) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, primaryRequest.target,
            primaryRequest.resolvedFrame, true);
        setSecondaryActiveRequest(target, resolvedFrame, true);

        if (dispatchNow) {
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest(ImageViewport::PageRole::Secondary, { target });
            result.providerFrameTransport = dispatch.transport;
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!dispatch.accepted) {
                result.changes.diagnostics = true;
            }
        } else {
            publishProviderFrameLoadingState(ImageViewport::PageRole::Secondary);
        }

        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsValueChanged;
        return result;
    };

    if (state.secondaryProvider.metadataReady) {
        if (!state.secondaryProvider.frameSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int maximumFrame = state.secondaryProvider.timedMetadata
            ? state.secondaryProvider.timingIntervals.frameCount() - 1
            : 0;
        if (frame > maximumFrame) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        const int position = state.secondaryProvider.timedMetadata
            ? state.secondaryProvider.timingIntervals.frameStartPosition(frame)
            : -1;
        return acceptTarget(
            { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { frame, position }, true);
    }

    if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport frameSeekCapability
            = viewport.secondaryProviderFrameSeekCapability();
        if (ImageViewportInternal::providerCapabilityKnownFalse(frameSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts = viewport.secondaryProviderKnownFacts();
        if (ImageViewportInternal::providerCapabilityKnownTrue(frameSeekCapability)
            && knownFacts.frameCount() >= 0 && frame >= knownFacts.frameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptTarget({ frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { -1, -1 }, false);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    ImageViewportInternal::CommandOutcome::markRejected(
        viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerPositionSeekCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { -1, -1 }, ExplicitSeekMaterialization::ProviderPendingMetadata);
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        if (!viewportProviderState(viewport).positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = viewport.providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { frame, viewport.providerFrameStartPosition(frame) },
            ExplicitSeekMaterialization::ProviderReady);
    }

    if (viewport.hasTimedSequence()) {
        const int frame = viewport.sequenceFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport, DisplayRequestTarget { frame, milliseconds },
            { frame, viewport.sequenceFrameStartPosition(frame) },
            ExplicitSeekMaterialization::BuiltIn);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    ImageViewportInternal::CommandOutcome::markRejected(
        viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekToPosition(
    ImageViewport::PageRole role, int milliseconds)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (role == ImageViewport::PageRole::Primary) {
        return seekToPosition(milliseconds);
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (viewportRequestState(viewport).secondarySequenceIsProvider) {
        return commandResultWithSecondaryTransport(seekSecondaryProviderToPosition(milliseconds));
    }
    if (milliseconds < 0) {
        return rejectInvalidCommand();
    }
    if (!state.secondarySource.timed) {
        return rejectUnsupportedCommand();
    }

    const int frame = frameIndexForRoleSource(viewport, state.secondarySource, milliseconds);
    if (frame < 0) {
        return rejectInvalidCommand();
    }
    const int frameStart = frameStartPositionForRoleSource(viewport, state.secondarySource, frame);
    return seekSecondaryBuiltIn(
        { frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
        { frame, frameStart });
}

ViewportCommandResult ViewportController::seekSecondaryProviderToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest() || !hasSecondaryProviderSequence(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        ImageViewportInternal::CommandOutcome::markRejected(
            viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto acceptTarget = [this](ImageViewportInternal::DisplayRequestTarget target,
                                  ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
                                  bool dispatchNow) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, primaryRequest.target,
            primaryRequest.resolvedFrame, true);
        setSecondaryActiveRequest(target, resolvedFrame, true);

        if (dispatchNow) {
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest(ImageViewport::PageRole::Secondary, { target });
            result.providerFrameTransport = dispatch.transport;
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!dispatch.accepted) {
                result.changes.diagnostics = true;
            }
        } else {
            publishProviderFrameLoadingState(ImageViewport::PageRole::Secondary);
        }

        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsValueChanged;
        return result;
    };

    if (!state.secondaryProvider.metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport positionSeekCapability
            = viewport.secondaryProviderPositionSeekCapability();
        if (ImageViewportInternal::providerCapabilityKnownFalse(positionSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts = viewport.secondaryProviderKnownFacts();
        if (ImageViewportInternal::providerCapabilityKnownTrue(positionSeekCapability)) {
            if (knownFacts.isStill()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (knownFacts.isTimedFrameList()
                && TimingIntervals::fromFrameDurations(knownFacts.frameDurations())
                        .frameIndexForPosition(milliseconds)
                    < 0) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }
        }

        return acceptTarget(
            { -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { -1, -1 }, false);
    }

    if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
        if (!state.secondaryProvider.positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame
            = state.secondaryProvider.timingIntervals.frameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }
        const int frameStart = state.secondaryProvider.timingIntervals.frameStartPosition(frame);
        return acceptTarget(
            { frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { frame, frameStart }, true);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    ImageViewportInternal::CommandOutcome::markRejected(
        viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

int ViewportController::playbackTimerInterval() const
{
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Playing
        || viewportRequestState(viewport).status != ImageViewport::RequestStatus::Ready) {
        return -1;
    }

    const ImageViewport::PageRole role = viewportRequestState(viewport).playbackRole;
    const ViewportPlaybackRoleTiming timing = playbackTimingForRole(viewport, state, role);
    if (!timing.valid) {
        return -1;
    }
    const int currentFrame = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
    if (currentFrame < 0 || currentFrame >= timing.frameCount) {
        return -1;
    }

    const int frameStart = timing.frameStartPosition(currentFrame);
    const int nextFrameStart = currentFrame + 1 < timing.frameCount
        ? timing.frameStartPosition(currentFrame + 1)
        : timing.totalDuration;
    const int frameDuration = nextFrameStart - frameStart;

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
        ? viewportRequestState(viewport).playbackPosition
        : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

ViewportPlaybackAdvanceResult ViewportController::advancePlayback(int elapsedMilliseconds)
{
    ViewportPlaybackAdvanceResult result;
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Playing
        || elapsedMilliseconds <= 0) {
        return result;
    }

    const ImageViewport::PageRole role = viewportRequestState(viewport).playbackRole;
    const ViewportPlaybackRoleTiming timing = playbackTimingForRole(viewport, state, role);
    if (!timing.valid) {
        return result;
    }
    const int previousFrame = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
    const int currentFrame = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
        effectiveLoopingForPlayback(viewport, timing.authoredAnimationFacts),
        timing.totalDuration, timing.frameCount,
        [&timing](int frame) { return timing.frameStartPosition(frame); },
        [&timing](int position) { return timing.frameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    viewportRequestState(viewport).playbackPosition = target.playbackPosition;

    if (timing.provider) {
        const bool sameReadyFrame = target.displayTarget.frame == currentFrame
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready;
        if (sameReadyFrame && role == ImageViewport::PageRole::Primary) {
            if (viewportRequestState(viewport).stopPlaybackWhenRequestReady) {
                setPlaybackPhase(result.changes, ImageViewport::PlaybackPhase::Stopped);
                viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
            } else {
                applyPlaybackAdvancePhase(viewport, result.changes, target);
            }
            return result;
        }
        if (sameReadyFrame && !target.reachedEnd && !target.looped) {
            return result;
        }

        const DisplayRequestTarget providerTarget {
            target.displayTarget.frame,
            target.displayTarget.position,
            ImageViewportInternal::ProviderRequestTargetKind::Playback,
        };
        beginPlaybackDisplayRequestForRole(*this, viewport, role,
            ImageViewportInternal::DisplayRequestOrigin::Playback, providerTarget,
            { providerTarget.frame, timing.frameStartPosition(providerTarget.frame) }, false);
        publishProviderPlaybackAdvanceLoadingState(*this, viewport, role);
        const bool diagnosticsValueChanged = role == ImageViewport::PageRole::Primary
            ? viewportRequestState(viewport).clearDiagnostics()
            : false;
        const ViewportProviderFrameDispatchResult dispatch
            = dispatchProviderFrameRequest(role, { providerTarget });
        setPlaybackProviderFrameTransportForRole(result, role, dispatch.transport);
        appendPlaybackRequestChange(viewport, result.changes, role, previousFrame);
        if (!dispatch.accepted) {
            result.changes.diagnostics = true;
            result.changes.scheduleUpdate = true;
            return result;
        }

        applyPlaybackAdvancePhase(viewport, result.changes, target);
        updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        return result;
    }

    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    beginPlaybackDisplayRequestForRole(*this, viewport, role,
        ImageViewportInternal::DisplayRequestOrigin::Playback, target.displayTarget,
        { target.displayTarget.frame, target.displayTarget.position }, false);
    publishAcceptedTargetState();
    updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
    applyPlaybackAdvancePhase(viewport, result.changes, target);
    appendPlaybackRequestChange(viewport, result.changes, role, previousFrame);
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.scheduleUpdate = true;
    return result;
}
