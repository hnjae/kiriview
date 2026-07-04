#include "viewportcontrollerhelpers_p.h"

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
            stageBuiltInPrimarySpreadPayload(viewport);
            publishPendingRenderState(viewport);
            changes.requestRevision = true;
            changes.displayRevision = true;
            changes.requestState = true;
            changes.displayState = true;
            changes.scheduleUpdate = true;
            return changes;
        }
    } else if (viewport.hasProviderSequence()
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading
        && viewportRequestState(viewport).reason == ImageViewport::RequestReason::UploadPending
        && viewport.itemBounds().isEmpty()
        && !viewportDisplayState(viewport).pendingRenderPayload.image.isNull()) {
        viewportRequestState(viewport).reason = ImageViewport::RequestReason::RenderWaiting;
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

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(
    double devicePixelRatio)
{
    ViewportRenderSynchronization synchronization;
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
        synchronization.oldContentRect = viewport.contentRect();
        synchronization.oldVisibleImageRect = viewport.visibleImageRect();
        synchronization.oldDisplayStatus = viewportDisplayState(viewport).status;
        synchronization.geometryState = controllerGeometryState(viewport, state.presentation,
            devicePixelRatio, std::nullopt, GeometryProjectionTarget::CurrentDisplay);
        synchronization.renderSnapshot
            = renderSnapshotForSynchronization(viewport, synchronization, state.presentation);
        return synchronization;
    }
    synchronization.pendingTargetCommit = requestIsWaitingForRenderCommit(viewport)
        && hasPendingTargetSpreadPayload(viewport) && !viewport.itemBounds().isEmpty();
    synchronization.pendingSecondaryProviderCommit = synchronization.pendingTargetCommit
        && hasSecondaryProviderSequence(viewport)
        && !viewportDisplayState(viewport).secondaryPendingRenderPayload.image.isNull();
    synchronization.oldContentRect = viewport.contentRect();
    synchronization.oldVisibleImageRect = viewport.visibleImageRect();
    synchronization.oldDisplayStatus = viewportDisplayState(viewport).status;
    if (synchronization.pendingTargetCommit) {
        synchronization.preparedPayload = viewportDisplayState(viewport).pendingRenderPayload;
    } else if (viewportDisplayState(viewport).pendingRenderPayload.commitPending
        && viewport.hasReadyDisplay()) {
        synchronization.preparedPayload = viewportDisplayState(viewport).pendingRenderPayload;
        synchronization.preparedPayload.image = viewportDisplayState(viewport).displayedImage;
    }
    synchronization.geometryState
        = controllerGeometryState(viewport, state.presentation, devicePixelRatio, std::nullopt,
            synchronization.pendingTargetCommit ? GeometryProjectionTarget::PendingRender
                                                : GeometryProjectionTarget::CurrentDisplay);
    synchronization.renderSnapshot
        = renderSnapshotForSynchronization(viewport, synchronization, state.presentation);
    return synchronization;
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderCommit(
    const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    ImageViewportInternal::ViewportChangeSet changes;
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
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
        publishSequenceReadyState(viewport, synchronization.preparedPayload);
    } else if (synchronization.pendingTargetCommit) {
        publishStagedBuiltInPrimarySpreadReadyState(viewport);
    }
    if (synchronization.pendingSecondaryProviderCommit) {
        viewportDisplayState(viewport).secondaryDisplayedImage
            = viewportDisplayState(viewport).secondaryPendingRenderPayload.image;
        viewportDisplayState(viewport).secondaryDisplayedImageSize
            = state.secondaryProvider.logicalSize;
    }
    const bool resumePlaybackAfterCommit
        = viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready;
    viewportDisplayState(viewport).commitDisplayedRequestSnapshot(
        viewportRequestState(viewport).sequenceGeneration,
        viewportRequestState(viewport).activeRequest,
        viewportDisplayState(viewport).pendingRenderPayload.payloadId);
    viewportDisplayState(viewport).clearPendingRenderPayload();
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    if (resumePlaybackAfterCommit) {
        setPlaybackPhase(viewport, changes,
            viewportRequestState(viewport).stopPlaybackWhenRequestReady
                ? ImageViewport::PlaybackPhase::Stopped
                : ImageViewport::PlaybackPhase::Playing);
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    }
    if (synchronization.pendingTargetCommit) {
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.requestState = true;
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
    if (targetSpreadTerminalSealedForActiveRequest(viewport)) {
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

    const ImageViewportInternal::PreparedPayloadIdentity failedPayload
        = acknowledgementPayloadForRole(acknowledgement, acknowledgement.failedRole);
    viewportRequestState(viewport).lastAcceptedRenderFailure = {
        true,
        acknowledgement.failedRole,
        failedPayload.generation,
        failedPayload.requestId,
        failedPayload.payloadId,
        acknowledgement.failureCause,
    };

    viewportDisplayState(viewport).clearPendingRenderPayload();
    if (viewportDisplayState(viewport).renderFailureRetainedDisplayValid) {
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Retained;
        viewportDisplayState(viewport).displayedRequest
            = viewportDisplayState(viewport).renderFailureRetainedRequest;
        viewportDisplayState(viewport).displayedImageSize
            = viewportDisplayState(viewport).renderFailureRetainedImageSize;
        viewportDisplayState(viewport).displayedImage
            = viewportDisplayState(viewport).renderFailureRetainedImage;
    } else {
        viewportDisplayState(viewport).status = ImageViewport::DisplayStatus::Empty;
        viewportDisplayState(viewport).clearDisplayedDisplay();
    }
    viewportDisplayState(viewport).clearRenderFailureRetainedDisplay();
    recordTargetSpreadTerminal(viewport, acknowledgement.failedRole,
        ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::RenderFailure,
        ImageViewportInternal::FailureScope::DisplayRequest, QStringLiteral("render commit failed"),
        changes);
    setPlaybackPhase(viewport, changes, ImageViewport::PlaybackPhase::Stopped);

    changes.displayRevision = true;
    changes.displayState = viewportDisplayState(viewport).status != oldDisplayStatus;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    return changes;
}
