#include "viewportcontrollerrenderhelpers_p.h"

ViewportEngine::GeometryChangeResult ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    return state.engine.handleGeometryChanged({ viewport.itemBounds(), oldContentRect,
        oldVisibleImageRect, controllerGeometryState(viewport, state.engine.presentationState()) });
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(
    double devicePixelRatio)
{
    return state.engine.beginRenderSynchronization({ QSizeF(viewport.width(), viewport.height()),
        viewport.itemBounds(), viewport.contentRect(), viewport.visibleImageRect(),
        controllerGeometryInput(viewport, devicePixelRatio, std::nullopt,
            GeometryProjectionTarget::CurrentDisplay),
        controllerGeometryInput(viewport, devicePixelRatio, std::nullopt,
            GeometryProjectionTarget::PendingRender) });
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderCommit(
    const ViewportRenderAcknowledgement& acknowledgement, bool renderedImagePresent,
    const ViewportRenderSynchronization& synchronization)
{
    return state.engine.acknowledgeRenderCommit({ acknowledgement, renderedImagePresent,
        synchronization.attempt, synchronization.pendingTargetCommit,
        synchronization.pendingSecondaryProviderCommit, synchronization.preparedPayload,
        synchronization.oldDisplayStatus, synchronization.oldContentRect,
        synchronization.oldVisibleImageRect, synchronization.geometryState });
}

ImageViewportInternal::ViewportChangeSet ViewportController::acknowledgeRenderFailure(
    const ViewportRenderAcknowledgement& acknowledgement)
{
    return state.engine.acknowledgeRenderFailure({ acknowledgement });
}
