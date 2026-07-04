#include "viewportcontrollerhelpers_p.h"

PresentationGeometry::State ViewportController::geometryState(double devicePixelRatio) const
{
    return controllerGeometryState(viewport, state.presentation, devicePixelRatio);
}

PresentationGeometry::State ViewportController::geometryStateForItemBounds(
    const QRectF& itemBounds, double devicePixelRatio) const
{
    return controllerGeometryState(viewport, state.presentation, devicePixelRatio, itemBounds);
}
