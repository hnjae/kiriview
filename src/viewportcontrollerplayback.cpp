#include "imageviewportvalidation_p.h"
#include "viewportcommandoutcome_p.h"
#include "viewportcontrollerplaybackhelpers_p.h"

namespace {
constexpr ImageViewport::PageRole primaryRole = ImageViewport::PageRole::Primary;
constexpr ImageViewport::PageRole secondaryRole = ImageViewport::PageRole::Secondary;

void clearQueuedProviderFrameRequest(ViewportControllerState& state, ImageViewport::PageRole role)
{
    ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, role);
    provider.queuedFrameRequest = false;
    provider.queuedFrameGeneration = 0;
    provider.queuedFrameRequestId = 0;
    provider.queuedFrame = -1;
    provider.queuedPosition = -1;
    provider.queuedResolvedFrame = {};
    provider.queuedFrameFromPlayback = false;
    provider.queuedFrameTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
}

DisplayRequestTarget providerLatestNonPlaybackTarget(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    const ImageViewportInternal::DisplayRequest& latestNonPlaybackRequest
        = latestNonPlaybackRequestForRole(viewportRequestState(viewport), role);
    DisplayRequestTarget target;
    target.frame = latestNonPlaybackRequest.target.frame;
    target.position = latestNonPlaybackRequest.target.position;
    target.providerTargetKind = latestNonPlaybackRequest.target.providerTargetKind;
    return target;
}

DisplayRequestTarget providerStopRestoreTarget(ViewportControllerPort& viewport,
    const ViewportControllerState& state, ImageViewport::PageRole role)
{
    const ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), role);
    DisplayRequestTarget target = providerLatestNonPlaybackTarget(viewport, role);
    if (target.frame < 0 && target.position >= 0) {
        target.frame = providerFrameIndexForPositionForRole(state, role, target.position);
    }
    if (target.frame < 0 && target.position < 0 && activeRequest.target.frame >= 0) {
        target.frame = activeRequest.target.frame;
        target.position = providerFrameStartPositionForRole(state, role, target.frame);
    }
    if (target.position < 0 && target.frame >= 0) {
        target.position = providerFrameStartPositionForRole(state, role, target.frame);
    }
    if (target.providerTargetKind == ImageViewportInternal::ProviderRequestTargetKind::Unknown
        && target.frame >= 0) {
        target.providerTargetKind = ImageViewportInternal::ProviderRequestTargetKind::Frame;
    }
    return target;
}

ImageViewportInternal::ResolvedFrameIdentity providerStopRestoreResolvedFrame(
    ViewportControllerPort& viewport, const ViewportControllerState& state,
    ImageViewport::PageRole role, DisplayRequestTarget target)
{
    const ImageViewportInternal::DisplayRequest& latestNonPlaybackRequest
        = latestNonPlaybackRequestForRole(viewportRequestState(viewport), role);
    if (latestNonPlaybackRequest.resolvedFrame.isValid()
        && latestNonPlaybackRequest.resolvedFrame.frame == target.frame) {
        return latestNonPlaybackRequest.resolvedFrame;
    }
    if (target.frame < 0) {
        return {};
    }
    return { target.frame, providerFrameStartPositionForRole(state, role, target.frame) };
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
    const auto primaryDisplay = displayRoleStateFor(viewportDisplayState(viewport), primaryRole);
    const ImageViewportInternal::DisplayRequest& primaryRequest
        = activeRequestForRole(viewportRequestState(viewport), primaryRole);
    return viewport.hasReadyDisplay()
        && primaryDisplay.displayedRequest.generation
        == viewportRequestState(viewport).sequenceGeneration
        && primaryDisplay.displayedRequest.request.resolvedFrame.frame
        == primaryRequest.resolvedFrame.frame
        && primaryDisplay.displayedRequest.request.resolvedFrame.position
        == primaryRequest.resolvedFrame.position;
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

StopRestorePlan stopRestorePlanFor(
    ViewportControllerPort& viewport, const ViewportControllerState& state)
{
    const ConstViewportProviderRoleState provider = providerRoleStateFor(state, primaryRole);
    if (viewport.hasProviderSequence() && !provider.provider.metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && provider.activeRequest.target.frame < 0 && provider.activeRequest.target.position < 0) {
        return { StopRestorePlanKind::ProviderPendingMetadata,
            providerLatestNonPlaybackTarget(viewport, primaryRole),
            provider.latestNonPlaybackRequest.resolvedFrame };
    }
    if (viewport.hasProviderSequence() && provider.provider.timedMetadata
        && provider.provider.queuedFrameRequest && provider.provider.queuedFrameFromPlayback) {
        const DisplayRequestTarget target = providerStopRestoreTarget(viewport, state, primaryRole);
        return { StopRestorePlanKind::ProviderQueuedPlayback, target,
            providerStopRestoreResolvedFrame(viewport, state, primaryRole, target) };
    }
    if (viewport.hasProviderSequence() && provider.provider.timedMetadata
        && activeProviderFrameRequestIsPlayback(viewport)) {
        const DisplayRequestTarget target = providerStopRestoreTarget(viewport, state, primaryRole);
        return { StopRestorePlanKind::ProviderActivePlayback, target,
            providerStopRestoreResolvedFrame(viewport, state, primaryRole, target) };
    }
    if (viewport.hasTimedSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting
        && (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
            || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Paused)
        && provider.latestNonPlaybackRequest.target.frame >= 0
        && provider.activeRequest.target.frame != provider.latestNonPlaybackRequest.target.frame) {
        return { StopRestorePlanKind::BuiltInRenderWait,
            DisplayRequestTarget { provider.latestNonPlaybackRequest.target.frame,
                provider.latestNonPlaybackRequest.target.position },
            provider.latestNonPlaybackRequest.resolvedFrame };
    }
    return {};
}

StopRestorePublication publishStopRestoreTarget(ViewportController& controller,
    ViewportControllerPort& viewport, DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
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
    markRequestMutation(result.changes);
}

bool appendProviderStopRestoreFrameStart(ViewportController& controller,
    ViewportControllerPort& viewport, const ViewportControllerState& state,
    ViewportCommandResult& result)
{
    const ConstViewportProviderRoleState provider = providerRoleStateFor(state, primaryRole);
    if (!provider.provider.session || provider.activeRequest.target.frame < 0) {
        return true;
    }

    DisplayRequestTarget target = provider.activeRequest.target;
    target.providerTargetKind = provider.latestNonPlaybackRequest.target.providerTargetKind;
    const ViewportProviderFrameRequestStartResult start
        = controller.startProviderFrameRequest({ target });
    appendProviderFrameStartResult(result.providerFrameTransport, start);
    if (start.accepted) {
        return true;
    }

    markProviderDispatchFailure(result.changes);
    return false;
}

bool applyStopRestorePlan(ViewportController& controller, ViewportControllerPort& viewport,
    ViewportControllerState& state, StopRestorePlan plan, ViewportCommandResult& result)
{
    switch (plan.kind) {
    case StopRestorePlanKind::ProviderPendingMetadata:
        applyProviderStopRestoreTarget(viewport, plan.target, plan.resolvedFrame);
        viewportRequestState(viewport).providerPlaybackStartPending = false;
        completeStopRestoreRequest(controller, viewport, result);
        return true;
    case StopRestorePlanKind::ProviderQueuedPlayback: {
        clearQueuedProviderFrameRequest(state, primaryRole);

        const StopRestorePublication publication = publishStopRestoreTarget(controller, viewport,
            plan.target, plan.resolvedFrame, StopRestoreWaitingState::ProviderLoading);
        if (!publication.readyDisplay) {
            if (!appendProviderStopRestoreFrameStart(controller, viewport, state, result)) {
                return true;
            }
        }
        completeStopRestoreRequest(controller, viewport, result);
        return true;
    }
    case StopRestorePlanKind::ProviderActivePlayback: {
        ImageViewportInternal::ProviderGenerationState& provider
            = providerGenerationStateForRole(state, primaryRole);
        if (provider.session) {
            result.providerFrameTransport.cancelToken = provider.activeFrameToken;
        }
        provider.activeFrameToken = {};

        const StopRestorePublication publication = publishStopRestoreTarget(controller, viewport,
            plan.target, plan.resolvedFrame, StopRestoreWaitingState::ProviderLoading);
        if (publication.readyDisplay) {
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            completeStopRestoreRequest(controller, viewport, result);
            markDisplayMutation(result.changes);
            markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
            return true;
        }
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        if (!appendProviderStopRestoreFrameStart(controller, viewport, state, result)) {
            return true;
        }
        completeStopRestoreRequest(controller, viewport, result);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        return true;
    }
    case StopRestorePlanKind::BuiltInRenderWait: {
        const StopRestorePublication publication = publishStopRestoreTarget(controller, viewport,
            plan.target, plan.resolvedFrame, StopRestoreWaitingState::RenderWaiting);
        completeStopRestoreRequest(controller, viewport, result);
        result.changes.displayRevision
            = viewportDisplayState(viewport).status != publication.oldDisplayStatus;
        result.changes.displayState = result.changes.displayRevision;
        markScheduleUpdate(result.changes);
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
    controller.beginAcceptedDisplayRequest(
        ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, target, resolvedFrame, true);
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
            markProviderDispatchFailure(result.changes);
            markDisplayMutation(result.changes);
            markScheduleUpdate(result.changes);
            return result;
        }
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing) {
            controller.setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
        }
        markRequestAndDisplayMutation(result.changes);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        markScheduleUpdate(result.changes);
        return result;
    }
    case ExplicitSeekMaterialization::ProviderPendingMetadata: {
        viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
        controller.discardPendingRenderCommit();
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        markRequestMutation(result.changes);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
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
        markRequestAndDisplayMutation(result.changes);
        result.changes.geometryState
            = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        markScheduleUpdate(result.changes);
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
    ImageViewportInternal::DisplayRequest& activeRequest
        = activeRequestForRole(viewportRequestState(viewport), primaryRole);
    activeRequest.target.frame = target.frame;
    activeRequest.target.position = target.position;
    activeRequest.resolvedFrame
        = { target.frame, viewport.providerFrameStartPosition(target.frame) };
    viewportRequestState(viewport).playbackPosition = target.position;
    activeRequest.target.providerTargetKind = target.providerTargetKind;
}

void applyPendingProviderPlaybackTarget(
    ViewportControllerPort& viewport, DisplayRequestTarget target)
{
    applyPendingProviderPlaybackTargetForRole(viewport, ImageViewport::PageRole::Primary, target);
}

void setActiveDisplayRequestForRole(ViewportControllerPort& viewport, ImageViewport::PageRole role,
    ImageViewportInternal::DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame, bool rememberAsLatestNonPlayback)
{
    ImageViewportInternal::RequestState& request = viewportRequestState(viewport);
    const ImageViewportInternal::DisplayRequest& primaryRequest
        = activeRequestForRole(request, primaryRole);
    ImageViewportInternal::DisplayRequest& activeRequest = activeRequestForRole(request, role);
    activeRequest.identity = primaryRequest.identity;
    activeRequest.target = target;
    activeRequest.resolvedFrame = resolvedFrame;
    activeRequest.providerFrameToken = {};
    activeRequest.preparedPayloadId = primaryRequest.preparedPayloadId;
    if (rememberAsLatestNonPlayback && target.frame >= 0) {
        latestNonPlaybackRequestForRole(request, role) = activeRequest;
    }
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
    setActiveDisplayRequestForRole(
        viewport, role, target, resolvedFrame, rememberAsLatestNonPlayback);
}

template <typename FrameStartFor>
int playbackStartPosition(
    ViewportControllerPort& viewport, ImageViewport::PageRole role, FrameStartFor frameStartFor)
{
    const auto& target = activeRequestForRole(viewportRequestState(viewport), role).target;
    return target.position >= 0 ? target.position : frameStartFor(target.frame);
}

template <typename FrameStartFor>
void seedPlaybackPosition(
    ViewportControllerPort& viewport, ImageViewport::PageRole role, FrameStartFor frameStartFor)
{
    viewportRequestState(viewport).playbackPosition
        = playbackStartPosition(viewport, role, frameStartFor);
}

void applyPlaybackAdvancePhase(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes, const PlaybackAdvanceTarget& target)
{
    if (target.reachedEnd) {
        viewportRequestState(viewport).stopPlaybackWhenRequestReady
            = viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading;
    }
    const ImageViewport::PlaybackPhase phase
        = playbackAdvancePhaseForRequest(viewportRequestState(viewport).status, target.reachedEnd);
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
    markRequestMutation(changes);
    if (activeRequestForRole(viewportRequestState(viewport), role).target.frame != previousFrame
        || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
        changes.displayRevision = true;
    }
    changes.displayState = true;
}

void publishProviderPlaybackAdvanceLoadingState(
    ViewportController& controller, ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Primary) {
        controller.publishProviderFrameLoadingState();
        return;
    }

    viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
    viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
    const auto primaryDisplay = displayRoleStateFor(viewportDisplayState(viewport), primaryRole);
    viewportDisplayState(viewport).status = primaryDisplay.displayedImageSize.isValid()
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
    const ImageViewportInternal::DisplayRequest& primaryRequest
        = activeRequestForRole(viewportRequestState(viewport), primaryRole);
    const bool currentIdentity
        = request.identity.id != 0 && request.identity.id == primaryRequest.identity.id;
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
        markRequestMutation(result.changes);
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
        const ImageViewportInternal::ProviderGenerationState& provider
            = providerGenerationStateForRole(state, primaryRole);
        if (!provider.metadataReady) {
            applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
            viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Waiting;
            return;
        }
        if (provider.timedMetadata && provider.timedPlaybackSupport) {
            seedPlaybackPosition(viewport, primaryRole,
                [this](int frame) { return viewport.providerFrameStartPosition(frame); });
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
    seedPlaybackPosition(viewport, primaryRole,
        [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
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

    const ImageViewportInternal::ProviderGenerationState& primaryProvider
        = providerGenerationStateForRole(state, primaryRole);
    if (viewport.hasProviderSequence() && primaryProvider.metadataReady
        && primaryProvider.timedMetadata && primaryProvider.timedPlaybackSupport) {
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
            const DisplayRequestTarget target
                = providerPlaybackStartTarget(viewport, state, primaryRole);
            ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            applyProviderPlaybackStartTarget(viewport, target);
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            publishProviderFrameLoadingState();
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest({ target });
            result.providerFrameTransport = dispatch.transport;
            if (!dispatch.accepted) {
                markProviderDispatchFailure(result.changes);
                return result;
            }
            setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
            markRequestMutation(result.changes);
            markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
            return result;
        }

        ImageViewportInternal::CommandOutcome::markAccepted(viewport, result);
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(viewport, primaryRole,
                [this](int frame) { return viewport.providerFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    if (viewport.hasProviderSequence() && !primaryProvider.metadataReady
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
        markRequestMutation(result.changes);
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
            seedPlaybackPosition(viewport, primaryRole,
                [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            setPlaybackPhase(result, playbackPhaseForCurrentRequest(viewport));
            markRequestMutation(result.changes);
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
            markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
            markScheduleUpdate(result.changes);
            return result;
        }
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(viewport, primaryRole,
                [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
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
    if (role == primaryRole) {
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
    if (role == secondaryRole && !hasSecondaryRole(viewport)) {
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
    if (role == secondaryRole && !hasSecondaryRole(viewport)) {
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
    if (role == secondaryRole) {
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Stopped) {
            return result;
        }

        if (activeProviderFrameRequestIsPlayback(state, viewport, secondaryRole)) {
            const auto provider = providerRoleStateFor(state, role);
            result.secondaryProviderFrameTransport.cancelToken = provider.provider.activeFrameToken;
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
            markRequestMutation(result.changes);
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
            result.changes.scheduleUpdate = true;
            return result;
        }

        setPlaybackPhase(result, ImageViewport::PlaybackPhase::Stopped);
        return result;
    }

    const StopRestorePlan stopRestorePlan = stopRestorePlanFor(viewport, state);
    if (applyStopRestorePlan(*this, viewport, state, stopRestorePlan, result)) {
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
        const ImageViewportInternal::ProviderGenerationState& primaryProvider
            = providerGenerationStateForRole(state, primaryRole);
        if (viewport.hasProviderSequence() && primaryProvider.metadataReady) {
            if (!primaryProvider.frameSeekSupport) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            const int maximumFrame = primaryProvider.timedMetadata
                ? primaryProvider.timingIntervals.frameCount() - 1
                : 0;
            if (frame > maximumFrame) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                ImageViewportInternal::CommandOutcome::markRejected(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget { frame,
                    providerFrameStartPositionForRole(state, primaryRole, frame),
                    ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderReady);
        }

        if (viewport.hasProviderSequence() && !primaryProvider.metadataReady
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
    if (role == primaryRole) {
        return seek(frame);
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (hasProviderSequenceForRole(viewport, role)) {
        return commandResultWithSecondaryTransport(seekSecondaryProvider(frame));
    }
    if (frame < 0) {
        return rejectInvalidCommand();
    }

    const ViewportSequenceRoleSource& source = secondaryRoleSource(state);
    const int frameCount = source.frameCount;
    if (frameCount <= 0 || frame >= frameCount) {
        return rejectInvalidCommand();
    }

    const int position = frameStartPositionForRoleSource(viewport, source, frame);
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
        = activeRequestForRole(viewportRequestState(viewport), primaryRole);
    beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        primaryRequest.target, primaryRequest.resolvedFrame, true);
    setActiveDisplayRequestForRole(viewport, secondaryRole, target, resolvedFrame, true);
    publishAcceptedTargetState();
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        setPlaybackPhase(result, ImageViewport::PlaybackPhase::Waiting);
    }

    markRequestMutation(result.changes);
    const bool displayValueChanged = viewportDisplayState(viewport).status != oldDisplayStatus
        || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
    result.changes.displayRevision = displayValueChanged;
    result.changes.displayState = displayValueChanged;
    result.changes.geometryState
        = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
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
            = activeRequestForRole(viewportRequestState(viewport), primaryRole);
        beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
            primaryRequest.target, primaryRequest.resolvedFrame, true);
        setActiveDisplayRequestForRole(viewport, secondaryRole, target, resolvedFrame, true);

        if (dispatchNow) {
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest(ImageViewport::PageRole::Secondary, { target });
            result.providerFrameTransport = dispatch.transport;
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!dispatch.accepted) {
                markDiagnosticsMutation(result.changes);
            }
        } else {
            publishProviderFrameLoadingState(ImageViewport::PageRole::Secondary);
        }

        markRequestMutation(result.changes);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        return result;
    };

    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, secondaryRole);
    if (provider.metadataReady) {
        if (!provider.frameSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int maximumFrame
            = provider.timedMetadata ? provider.timingIntervals.frameCount() - 1 : 0;
        if (frame > maximumFrame) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        const int position = provider.timedMetadata
            ? providerFrameStartPositionForRole(state, secondaryRole, frame)
            : -1;
        return acceptTarget(
            { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { frame, position }, true);
    }

    if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport frameSeekCapability
            = providerFrameSeekCapabilityForRole(viewport, secondaryRole);
        if (ImageViewportInternal::providerCapabilityKnownFalse(frameSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts
            = providerKnownFactsForRole(viewport, secondaryRole);
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

    const ImageViewportInternal::ProviderGenerationState& primaryProvider
        = providerGenerationStateForRole(state, primaryRole);
    if (viewport.hasProviderSequence() && !primaryProvider.metadataReady
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

    if (viewport.hasProviderSequence() && primaryProvider.metadataReady
        && primaryProvider.timedMetadata) {
        if (!primaryProvider.positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = providerFrameIndexForPositionForRole(state, primaryRole, milliseconds);
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
            { frame, providerFrameStartPositionForRole(state, primaryRole, frame) },
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
    if (role == primaryRole) {
        return seekToPosition(milliseconds);
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (hasProviderSequenceForRole(viewport, role)) {
        return commandResultWithSecondaryTransport(seekSecondaryProviderToPosition(milliseconds));
    }
    if (milliseconds < 0) {
        return rejectInvalidCommand();
    }
    const ViewportSequenceRoleSource& source = secondaryRoleSource(state);
    if (!source.timed) {
        return rejectUnsupportedCommand();
    }

    const int frame = frameIndexForRoleSource(viewport, source, milliseconds);
    if (frame < 0) {
        return rejectInvalidCommand();
    }
    const int frameStart = frameStartPositionForRoleSource(viewport, source, frame);
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
            = activeRequestForRole(viewportRequestState(viewport), primaryRole);
        beginAcceptedDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
            primaryRequest.target, primaryRequest.resolvedFrame, true);
        setActiveDisplayRequestForRole(viewport, secondaryRole, target, resolvedFrame, true);

        if (dispatchNow) {
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest(ImageViewport::PageRole::Secondary, { target });
            result.providerFrameTransport = dispatch.transport;
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!dispatch.accepted) {
                markDiagnosticsMutation(result.changes);
            }
        } else {
            publishProviderFrameLoadingState(ImageViewport::PageRole::Secondary);
        }

        markRequestMutation(result.changes);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        return result;
    };

    const ImageViewportInternal::ProviderGenerationState& provider
        = providerGenerationStateForRole(state, secondaryRole);
    if (!provider.metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport positionSeekCapability
            = providerPositionSeekCapabilityForRole(viewport, secondaryRole);
        if (ImageViewportInternal::providerCapabilityKnownFalse(positionSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts
            = providerKnownFactsForRole(viewport, secondaryRole);
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

    if (provider.metadataReady && provider.timedMetadata) {
        if (!provider.positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = providerFrameIndexForPositionForRole(state, secondaryRole, milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            ImageViewportInternal::CommandOutcome::markRejected(
                viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }
        const int frameStart = providerFrameStartPositionForRole(state, secondaryRole, frame);
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
    const int currentFrame
        = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
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
    const int previousFrame
        = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
    const int currentFrame
        = activeRequestForRole(viewportRequestState(viewport), role).target.frame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
        effectiveLoopingForPlayback(viewport, timing.authoredAnimationFacts), timing.totalDuration,
        timing.frameCount, [&timing](int frame) { return timing.frameStartPosition(frame); },
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
            markDiagnosticsMutation(result.changes);
            markScheduleUpdate(result.changes);
            return result;
        }

        applyPlaybackAdvancePhase(viewport, result.changes, target);
        updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
        markDiagnosticsMutation(result.changes, diagnosticsValueChanged);
        markScheduleUpdate(result.changes);
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
        = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    markScheduleUpdate(result.changes);
    return result;
}
