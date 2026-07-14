#include "viewportcontrollerrenderhelpers_p.h"
#include "viewportprovidertransporteffects_p.h"

ViewportControllerTransition ViewportController::handleGeometryChanged(
    const QRectF& oldContentRect, const QRectF& oldVisibleImageRect)
{
    const auto engineResult
        = engine.handleGeometryChanged({ { itemBounds(), 1.0 }, oldContentRect,
            oldVisibleImageRect });
    ViewportControllerTransition result;
    result.changes = engineResult.changes;
    appendProviderTransport(result.providerAfterPublication, engineResult.providerEffects[0],
        ImageViewportPageRole::Primary);
    appendProviderTransport(result.providerAfterPublication, engineResult.providerEffects[1],
        ImageViewportPageRole::Secondary);
    return result;
}

ViewportRenderSynchronization ViewportController::beginRenderSynchronization(
    double devicePixelRatio)
{
    return engine.beginRenderSynchronization({ { itemBounds(), devicePixelRatio } });
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
