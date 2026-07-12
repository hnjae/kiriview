#include "viewportcontrollerrenderhelpers_p.h"
#include "viewportprovidertransporteffects_p.h"

ViewportControllerTransition ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    const auto engineResult = engine.handleGeometryChanged({ itemBounds(), oldContentRect,
        oldVisibleImageRect, engine.geometryState(engine.projectedGeometryInput(itemBounds())) });
    ViewportControllerTransition result;
    result.changes = engineResult.changes;
    appendProviderTransport(result.providerAfterPublication, engineResult.providerEffects[0],
        ImageViewport::PageRole::Primary);
    appendProviderTransport(result.providerAfterPublication, engineResult.providerEffects[1],
        ImageViewport::PageRole::Secondary);
    return result;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(
    double devicePixelRatio)
{
    const QRectF bounds = itemBounds();
    const auto current = engine.projectedGeometryInput(bounds, devicePixelRatio);
    const auto pending = engine.projectedGeometryInput(
        bounds, devicePixelRatio, ViewportEngine::GeometryProjectionTarget::PendingRender);
    const auto currentState = engine.geometryState(current);
    return engine.beginRenderSynchronization(
        { bounds.size(), bounds, PresentationGeometry::contentRect(currentState),
            PresentationGeometry::visibleImageRect(currentState), current, pending });
}

ViewportControllerTransition ViewportController::acknowledgeRenderCommit(
    const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    ViewportControllerTransition result;
    const auto transition = engine.acknowledgeRenderCommit({ acknowledgement, renderedImagePresent,
        synchronization.attempt, synchronization.pendingTargetCommit,
        synchronization.pendingSecondaryProviderCommit, synchronization.preparedPayload,
        synchronization.oldDisplayStatus, synchronization.oldContentRect,
        synchronization.oldVisibleImageRect, synchronization.geometryState });
    result.changes = transition.changes;
    result.playbackSchedule = transition.playbackSchedule;
    return result;
}

ViewportControllerTransition ViewportController::acknowledgeRenderFailure(
    const ViewportRenderAcknowledgement& acknowledgement)
{
    ViewportControllerTransition result;
    const auto transition = engine.acknowledgeRenderFailure({ acknowledgement });
    result.changes = transition.changes;
    result.playbackSchedule = transition.playbackSchedule;
    return result;
}
