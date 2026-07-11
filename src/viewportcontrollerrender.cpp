#include "viewportcontrollerrenderhelpers_p.h"
#include "viewportprovidertransporteffects_p.h"

ViewportController::GeometryChangeResult ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    const auto engineResult = engine.handleGeometryChanged({ itemBounds(), oldContentRect, oldVisibleImageRect,
        engine.geometryState(engine.projectedGeometryInput(itemBounds())) });
    GeometryChangeResult result;
    result.changes = engineResult.changes;
    appendProviderTransport(result.afterChanges, engineResult.providerEffects[0],
        ImageViewport::PageRole::Primary);
    appendProviderTransport(result.afterChanges, engineResult.providerEffects[1],
        ImageViewport::PageRole::Secondary);
    return result;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(
    double devicePixelRatio)
{
    const QRectF bounds = itemBounds();
    const auto current = engine.projectedGeometryInput(bounds, devicePixelRatio);
    const auto pending = engine.projectedGeometryInput(bounds, devicePixelRatio,
        ViewportEngine::GeometryProjectionTarget::PendingRender);
    const auto currentState = engine.geometryState(current);
    return engine.beginRenderSynchronization({ bounds.size(), bounds,
        PresentationGeometry::contentRect(currentState),
        PresentationGeometry::visibleImageRect(currentState), current, pending });
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderCommit(
    const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    return engine.acknowledgeRenderCommit({ acknowledgement, renderedImagePresent,
        synchronization.attempt, synchronization.pendingTargetCommit,
        synchronization.pendingSecondaryProviderCommit, synchronization.preparedPayload,
        synchronization.oldDisplayStatus, synchronization.oldContentRect,
        synchronization.oldVisibleImageRect, synchronization.geometryState });
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderFailure(
    const ViewportRenderAcknowledgement& acknowledgement)
{
    return engine.acknowledgeRenderFailure({ acknowledgement });
}
