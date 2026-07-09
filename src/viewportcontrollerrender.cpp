#include "viewportcontrollerrenderhelpers_p.h"

namespace {
ViewportRenderSnapshotInput renderSnapshotInputForSynchronization(
    ViewportControllerPort viewport, const ViewportRenderSynchronization& synchronization)
{
    return { QSizeF(viewport.width(), viewport.height()), synchronization.pendingTargetCommit,
        synchronization.preparedPayload, synchronization.geometryState };
}

bool secondaryPayloadReadyForPendingTarget(ViewportControllerPort& viewport)
{
    if (!targetRequiresSecondaryPayload(viewport)) {
        return true;
    }
    if (hasSecondarySequence(viewport)) {
        const auto secondaryDisplay = displayRoleStateFor(
            viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
        return !secondaryDisplay.pendingPayload.image.isNull();
    }
    return false;
}

bool hasPendingTargetSpreadPayload(ViewportControllerPort& viewport)
{
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    return primaryDisplay.pendingPayload.commitPending
        && !primaryDisplay.pendingPayload.image.isNull()
        && secondaryPayloadReadyForPendingTarget(viewport);
}

bool requestIsWaitingForRenderCommit(ViewportControllerPort& viewport)
{
    return viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason
                == ImageViewport::RequestReason::RenderWaiting);
}

ImageViewportInternal::PreparedPayloadIdentity acknowledgementPayloadForRole(
    const ViewportRenderAcknowledgement& acknowledgement, ImageViewport::PageRole role)
{
    for (const ViewportRenderRolePayload& rolePayload : acknowledgement.rolePayloads) {
        if (rolePayload.role == role) {
            return rolePayload.preparedPayload;
        }
    }
    return role == ImageViewport::PageRole::Primary
        ? acknowledgement.preparedPayload
        : ImageViewportInternal::PreparedPayloadIdentity {};
}

ImageViewportInternal::PreparedPayloadIdentity expectedRenderPayloadForRole(
    ViewportControllerPort& viewport, ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Primary) {
        return displayRoleStateFor(viewportDisplayState(viewport), role).pendingPayload.identity();
    }
    if (!hasSecondarySequence(viewport)) {
        return {};
    }
    const ImageViewportInternal::PreparedPayloadIdentity secondaryIdentity
        = displayRoleStateFor(viewportDisplayState(viewport), role).pendingPayload.identity();
    return secondaryIdentity.isValid()
        ? secondaryIdentity
        : displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary)
              .pendingPayload.identity();
}

bool renderPayloadMatches(ImageViewportInternal::PreparedPayloadIdentity actual,
    ImageViewportInternal::PreparedPayloadIdentity expected)
{
    return actual.isValid() && expected.isValid() && actual.generation == expected.generation
        && actual.requestId == expected.requestId && actual.payloadId == expected.payloadId;
}

bool primaryRenderAcknowledgementMatchesPending(
    ViewportControllerPort& viewport, const ViewportRenderAcknowledgement& acknowledgement)
{
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload
        = acknowledgementPayloadForRole(acknowledgement, ImageViewport::PageRole::Primary);
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    return primaryDisplay.pendingPayload.commitPending
        && renderPayloadMatches(primaryPayload, primaryDisplay.pendingPayload.identity())
        && viewportRequestState(viewport).activeRequestOwnsPreparedPayload(primaryPayload);
}

bool renderCommitAcknowledgementMatchesPending(
    ViewportControllerPort& viewport, const ViewportRenderAcknowledgement& acknowledgement)
{
    if (!primaryRenderAcknowledgementMatchesPending(viewport, acknowledgement)) {
        return false;
    }
    if (!hasSecondarySequence(viewport)) {
        return true;
    }
    return renderPayloadMatches(
        acknowledgementPayloadForRole(acknowledgement, ImageViewport::PageRole::Secondary),
        expectedRenderPayloadForRole(viewport, ImageViewport::PageRole::Secondary));
}

bool renderFailureAcknowledgementMatchesPending(
    ViewportControllerPort& viewport, const ViewportRenderAcknowledgement& acknowledgement)
{
    if (acknowledgement.failedRole == ImageViewport::PageRole::Primary) {
        return primaryRenderAcknowledgementMatchesPending(viewport, acknowledgement);
    }
    return renderPayloadMatches(
        acknowledgementPayloadForRole(acknowledgement, ImageViewport::PageRole::Secondary),
        expectedRenderPayloadForRole(viewport, ImageViewport::PageRole::Secondary));
}

void recordRenderFailureDiagnostic(ViewportControllerPort& viewport,
    ImageViewportInternal::ViewportChangeSet& changes,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    const ImageViewportInternal::PreparedPayloadIdentity failedPayload
        = acknowledgementPayloadForRole(acknowledgement, acknowledgement.failedRole);
    const ImageViewportInternal::RenderFailureDiagnostic renderFailureDiagnostic {
        true,
        acknowledgement.failedRole,
        failedPayload.generation,
        failedPayload.requestId,
        failedPayload.payloadId,
        acknowledgement.failureCause,
    };
    viewportRequestState(viewport).lastAcceptedRenderFailure = renderFailureDiagnostic;
    changes.renderFailureDiagnostic = renderFailureDiagnostic;
}
}

ImageViewportInternal::ViewportChangeSet ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (viewport.hasDisplayableSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && (viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
            || viewportRequestState(viewport).reason == ImageViewport::RequestReason::RenderWaiting)
        && !viewport.itemBounds().isEmpty()) {
        if (hasPendingTargetSpreadPayload(viewport)) {
            changes.scheduleUpdate = true;
            return changes;
        }
        if (!viewport.hasProviderSequence()) {
            stageBuiltInPrimarySpreadPayload();
            publishPendingRenderState();
            markRequestAndDisplayMutation(changes);
            markScheduleUpdate(changes);
            return changes;
        }
    } else if (viewport.hasProviderSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
        && viewport.itemBounds().isEmpty()
        && !displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary)
            .pendingPayload.image.isNull()) {
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::RenderWaiting;
        markRequestMutation(changes);
        changes.displayRevision = true;
    } else {
        changes.displayRevision = true;
    }

    changes.geometryState = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    markScheduleUpdate(changes);
    return changes;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(
    double devicePixelRatio)
{
    ViewportRenderSynchronization synchronization;
    if (targetSpreadTerminalSealedForActiveRequest()) {
        synchronization.oldContentRect = viewport.contentRect();
        synchronization.oldVisibleImageRect = viewport.visibleImageRect();
        synchronization.oldDisplayStatus = viewportDisplayState(viewport).status;
        synchronization.geometryState
            = controllerGeometryState(viewport, state.engine.presentationState(), devicePixelRatio,
                std::nullopt, GeometryProjectionTarget::CurrentDisplay);
        synchronization.renderSnapshot = state.engine.renderSnapshot(
            renderSnapshotInputForSynchronization(viewport, synchronization));
        return synchronization;
    }
    synchronization.pendingTargetCommit = requestIsWaitingForRenderCommit(viewport)
        && hasPendingTargetSpreadPayload(viewport) && !viewport.itemBounds().isEmpty();
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    const auto secondaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
    synchronization.pendingSecondaryProviderCommit = synchronization.pendingTargetCommit
        && hasSecondaryProviderSequence(viewport)
        && !secondaryDisplay.pendingPayload.image.isNull();
    synchronization.oldContentRect = viewport.contentRect();
    synchronization.oldVisibleImageRect = viewport.visibleImageRect();
    synchronization.oldDisplayStatus = viewportDisplayState(viewport).status;
    if (synchronization.pendingTargetCommit) {
        synchronization.preparedPayload = primaryDisplay.pendingPayload;
    } else if (primaryDisplay.pendingPayload.commitPending && viewport.hasReadyDisplay()) {
        synchronization.preparedPayload = primaryDisplay.pendingPayload;
        synchronization.preparedPayload.image = primaryDisplay.displayedImage;
    }
    synchronization.geometryState = controllerGeometryState(viewport,
        state.engine.presentationState(), devicePixelRatio, std::nullopt,
        synchronization.pendingTargetCommit ? GeometryProjectionTarget::PendingRender
                                            : GeometryProjectionTarget::CurrentDisplay);
    synchronization.renderSnapshot = state.engine.renderSnapshot(
        renderSnapshotInputForSynchronization(viewport, synchronization));
    return synchronization;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderCommit(
    const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return changes;
    }
    if (!renderedImagePresent) {
        return changes;
    }

    const bool renderMatchesPending
        = renderCommitAcknowledgementMatchesPending(viewport, acknowledgement);
    if (!renderMatchesPending) {
        return changes;
    }
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    if (synchronization.pendingTargetCommit && viewport.hasProviderSequence()) {
        publishSequenceReadyState(synchronization.preparedPayload);
    } else if (synchronization.pendingTargetCommit) {
        publishStagedBuiltInPrimarySpreadReadyState();
    }
    if (synchronization.pendingSecondaryProviderCommit) {
        ViewportDisplayRoleState secondaryDisplay = displayRoleStateFor(
            viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
        const auto secondaryProvider
            = providerRoleStateFor(state, ImageViewport::PageRole::Secondary);
        secondaryDisplay.displayedImage = secondaryDisplay.pendingPayload.image;
        secondaryDisplay.displayedImageSize = secondaryProvider.provider.logicalSize;
    }
    const bool resumePlaybackAfterCommit
        = viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready;
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    viewportDisplayState(viewport).commitDisplayedRequestSnapshot(
        viewportRequestState(viewport).sequenceGeneration,
        activeRequestForRole(viewportRequestState(viewport), ImageViewport::PageRole::Primary),
        primaryDisplay.pendingPayload.payloadId);
    viewportDisplayState(viewport).clearPendingRenderPayload();
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    if (resumePlaybackAfterCommit) {
        setPlaybackPhase(changes,
            viewportRequestState(viewport).stopPlaybackWhenRequestReady
                ? ImageViewport::PlaybackPhase::Stopped
                : ImageViewport::PlaybackPhase::Playing);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    }
    if (synchronization.pendingTargetCommit) {
        markRequestMutation(changes);
        changes.displayRevision = true;
        changes.displayState = viewportDisplayState(viewport).status
            != (viewport.hasProviderSequence() ? synchronization.oldDisplayStatus
                                               : oldDisplayStatus);
        changes.geometryState = ImageViewportInternal::rectsDifferExactly(
                                    viewport.contentRect(), synchronization.oldContentRect)
            || ImageViewportInternal::rectsDifferExactly(
                viewport.visibleImageRect(), synchronization.oldVisibleImageRect);
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderFailure(
    const ViewportRenderAcknowledgement& acknowledgement)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (targetSpreadTerminalSealedForActiveRequest()) {
        return changes;
    }
    const bool renderMatchesPending
        = renderFailureAcknowledgementMatchesPending(viewport, acknowledgement);
    const bool pendingTargetCommit
        = requestIsWaitingForRenderCommit(viewport) && hasPendingTargetSpreadPayload(viewport);
    if (!renderMatchesPending
        || (viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready
            && !pendingTargetCommit)) {
        return changes;
    }

    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;

    recordRenderFailureDiagnostic(viewport, changes, acknowledgement);

    viewportDisplayState(viewport).clearPendingRenderPayload();
    if (viewportDisplayState(viewport).renderFailureRetainedDisplayValid) {
        ViewportDisplayRoleState primaryDisplay
            = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Retained;
        primaryDisplay.displayedRequest
            = viewportDisplayState(viewport).renderFailureRetainedRequest;
        primaryDisplay.displayedImageSize
            = viewportDisplayState(viewport).renderFailureRetainedImageSize;
        primaryDisplay.displayedImage = viewportDisplayState(viewport).renderFailureRetainedImage;
    } else {
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        viewportDisplayState(viewport).clearDisplayedDisplay();
    }
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    recordTargetSpreadTerminal(acknowledgement.failedRole, ImageViewport::RequestStatus::Error,
        ImageViewport::RequestReason::RenderFailure,
        ImageViewportInternal::FailureScope::DisplayRequest, QStringLiteral("render commit failed"),
        changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);

    changes.displayRevision = true;
    changes.displayState = viewportDisplayState(viewport).status != oldDisplayStatus;
    changes.geometryState = viewportGeometryChanged(viewport, oldContentRect, oldVisibleImageRect);
    return changes;
}
