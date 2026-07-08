#include "viewportcontrollergeometryhelpers_p.h"

PresentationGeometry::State ViewportController::geometryState(double devicePixelRatio) const
{
    return controllerGeometryState(viewport, state.engine.presentationState(), devicePixelRatio);
}

PresentationGeometry::State ViewportController::geometryStateForItemBounds(
    const QRectF& itemBounds, double devicePixelRatio) const
{
    return controllerGeometryState(
        viewport, state.engine.presentationState(), devicePixelRatio, itemBounds);
}

double ViewportController::minimumManualZoomPercent() const
{
    return manualZoomMinimumPercentValue();
}

double ViewportController::maximumManualZoomPercent(double devicePixelRatio) const
{
    return manualZoomMaximumPercentValue(
        controllerGeometryState(viewport, state.engine.presentationState(), devicePixelRatio));
}

double ViewportController::manualZoomStepFactor() const { return manualZoomStepFactorValue(); }

double ViewportController::clampedManualZoomPercent(double percent, double devicePixelRatio) const
{
    return clampedManualZoomPercentValue(percent,
        controllerGeometryState(viewport, state.engine.presentationState(), devicePixelRatio));
}

double ViewportController::steppedManualZoomPercent(int stepCount, double devicePixelRatio) const
{
    return steppedManualZoomPercentValue(stepCount,
        controllerGeometryState(viewport, state.engine.presentationState(), devicePixelRatio));
}
