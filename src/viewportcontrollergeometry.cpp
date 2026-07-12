#include "viewportcontrollergeometryhelpers_p.h"
#include "viewportcontroller_p.h"

PresentationGeometry::State ViewportController::geometryState(double devicePixelRatio) const
{
    return engine.geometryState({ itemBounds(), devicePixelRatio });
}

PresentationGeometry::State ViewportController::geometryStateForItemBounds(
    const QRectF& itemBounds, double devicePixelRatio) const
{
    return engine.geometryState({ itemBounds, devicePixelRatio });
}

double ViewportController::minimumManualZoomPercent() const
{
    return manualZoomMinimumPercentValue();
}

double ViewportController::maximumManualZoomPercent(double devicePixelRatio) const
{
    return manualZoomMaximumPercentValue(
        engine.geometryState({ itemBounds(), devicePixelRatio }));
}

double ViewportController::manualZoomStepFactor() const { return manualZoomStepFactorValue(); }

double ViewportController::clampedManualZoomPercent(double percent, double devicePixelRatio) const
{
    return clampedManualZoomPercentValue(percent,
        engine.geometryState({ itemBounds(), devicePixelRatio }));
}

double ViewportController::steppedManualZoomPercent(int stepCount, double devicePixelRatio) const
{
    return steppedManualZoomPercentValue(stepCount,
        engine.geometryState({ itemBounds(), devicePixelRatio }));
}
