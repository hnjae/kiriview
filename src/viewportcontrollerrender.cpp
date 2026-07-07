#include "viewportcontrollerrenderhelpers_p.h"

namespace {
QRectF renderTargetRect(const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::pageItemRect(geometry, role).intersected(geometry.itemBounds);
}

QRectF renderSourceRect(const PresentationGeometry::State& geometry, ImageViewport::PageRole role)
{
    return PresentationGeometry::visiblePageRect(geometry, role);
}

ImageViewportInternal::PreparedPayload primaryRenderPayload(
    ViewportControllerPort viewport, const ViewportRenderSynchronization& synchronization)
{
    const auto primaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Primary);
    ImageViewportInternal::PreparedPayload payload = synchronization.preparedPayload;
    if (payload.image.isNull()
        && viewportDisplayState(viewport).hasReadyDisplay(viewport.hasDisplayableSequence())) {
        payload.image = primaryDisplay.displayedImage;
    }
    return payload;
}

ImageViewportInternal::PreparedPayload secondaryRenderPayload(ViewportControllerPort viewport,
    const ViewportRenderSynchronization& synchronization,
    const ImageViewportInternal::PreparedPayload& primaryPayload)
{
    const auto secondaryDisplay
        = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
    ImageViewportInternal::PreparedPayload payload = primaryPayload;
    if (synchronization.pendingTargetCommit && !secondaryDisplay.pendingPayload.image.isNull()) {
        return secondaryDisplay.pendingPayload;
    }
    payload.image = secondaryDisplay.displayedImage;
    return payload;
}

void appendRenderLayer(QVector<ViewportRenderLayer>& layers, ImageViewport::PageRole role,
    const ImageViewportInternal::PreparedPayload& payload, const QRectF& targetRect,
    const QRectF& sourceRect, const ImageViewportInternal::PresentationState& presentation,
    bool requirePresentableRects)
{
    if (payload.image.isNull()) {
        return;
    }
    if (requirePresentableRects && (targetRect.isEmpty() || sourceRect.isEmpty())) {
        return;
    }
    layers.append({ role, payload, targetRect, sourceRect, presentation.rotationDegrees,
        presentation.mirrorHorizontally, presentation.mirrorVertically });
}

ViewportRenderSnapshot renderSnapshotForSynchronization(ViewportControllerPort viewport,
    const ViewportRenderSynchronization& synchronization,
    const ImageViewportInternal::PresentationState& presentation)
{
    ViewportRenderSnapshot snapshot;
    snapshot.itemSize = QSizeF(viewport.width(), viewport.height());
    snapshot.backgroundMode = presentation.backgroundMode;
    snapshot.backgroundColor = presentation.backgroundColor;
    snapshot.smoothing = presentation.smoothing;
    snapshot.mipmap = presentation.mipmap;
    snapshot.rotationDegrees = presentation.rotationDegrees;
    snapshot.mirrorHorizontally = presentation.mirrorHorizontally;
    snapshot.mirrorVertically = presentation.mirrorVertically;

    const ImageViewportInternal::PreparedPayload primaryPayload
        = primaryRenderPayload(viewport, synchronization);
    snapshot.preparedPayload = primaryPayload;
    snapshot.targetRect
        = renderTargetRect(synchronization.geometryState, ImageViewport::PageRole::Primary);
    snapshot.sourceRect
        = renderSourceRect(synchronization.geometryState, ImageViewport::PageRole::Primary);
    appendRenderLayer(snapshot.imageLayers, ImageViewport::PageRole::Primary, primaryPayload,
        snapshot.targetRect, snapshot.sourceRect, presentation, false);

    const ImageViewportInternal::PreparedPayload secondaryPayload
        = secondaryRenderPayload(viewport, synchronization, primaryPayload);
    appendRenderLayer(snapshot.imageLayers, ImageViewport::PageRole::Secondary, secondaryPayload,
        renderTargetRect(synchronization.geometryState, ImageViewport::PageRole::Secondary),
        renderSourceRect(synchronization.geometryState, ImageViewport::PageRole::Secondary),
        presentation, true);
    return snapshot;
}

bool secondaryPayloadReadyForPendingTarget(ViewportControllerPort& viewport)
{
    if (!targetRequiresSecondaryPayload(viewport)) {
        return true;
    }
    if (hasSecondarySequence(viewport)) {
        const auto secondaryDisplay
            = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
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
    if (targetSpreadTerminalSealedForActiveRequest()) {
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
    } else if (primaryDisplay.pendingPayload.commitPending
        && viewport.hasReadyDisplay()) {
        synchronization.preparedPayload = primaryDisplay.pendingPayload;
        synchronization.preparedPayload.image = primaryDisplay.displayedImage;
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
        ViewportDisplayRoleState secondaryDisplay
            = displayRoleStateFor(viewportDisplayState(viewport), ImageViewport::PageRole::Secondary);
        const auto secondaryProvider
            = providerRoleStateFor(state, ImageViewport::PageRole::Secondary);
        secondaryDisplay.displayedImage = secondaryDisplay.pendingPayload.image;
        secondaryDisplay.displayedImageSize = secondaryProvider.provider.logicalSize;
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
        setPlaybackPhase(changes, viewportRequestState(viewport).stopPlaybackWhenRequestReady
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
    recordTargetSpreadTerminal(acknowledgement.failedRole,
        ImageViewport::RequestStatus::Error, ImageViewport::RequestReason::RenderFailure,
        ImageViewportInternal::FailureScope::DisplayRequest, QStringLiteral("render commit failed"),
        changes);
    setPlaybackPhase(changes, ImageViewport::PlaybackPhase::Stopped);

    changes.displayRevision = true;
    changes.displayState = viewportDisplayState(viewport).status != oldDisplayStatus;
    changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    return changes;
}
