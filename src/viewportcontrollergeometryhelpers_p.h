#pragma once


#include "presentationgeometry_p.h"
#include "viewportgeometryhelpers_p.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool isPositiveGeometrySize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}


QSizeF orientedSpreadSize(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = PresentationGeometry::spreadSize(state);
    const int rotation = ((state.rotationDegrees % 360) + 360) % 360;
    if (rotation == 90 || rotation == 270) {
        return QSizeF(spreadSize.height(), spreadSize.width());
    }
    return spreadSize;
}


double effectiveZoomPercent(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = orientedSpreadSize(state);
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || !isPositiveGeometrySize(spreadSize)) {
        return state.manualZoom * 100.0;
    }

    return content.width() / spreadSize.width() * state.devicePixelRatio * 100.0;
}

double manualZoomMinimumPercentValue()
{
    const double denormalMinimum = std::numeric_limits<double>::denorm_min();
    return denormalMinimum > 0.0 ? denormalMinimum : std::numeric_limits<double>::min();
}

double manualZoomStepFactorValue() { return 1.25; }

double manualZoomMaximumPercentValue(const PresentationGeometry::State& state)
{
    (void)state;
    return ImageViewportDisplayLimits::maximumManualZoomPercent();
}

double clampedManualZoomPercentValue(double percent, const PresentationGeometry::State& state)
{
    const double minimum = manualZoomMinimumPercentValue();
    const double maximum = manualZoomMaximumPercentValue(state);
    if (!std::isfinite(percent) || percent <= 0.0) {
        return minimum;
    }
    return std::clamp(percent, minimum, maximum);
}

double steppedManualZoomPercentValue(int stepCount, const PresentationGeometry::State& state)
{
    const double minimum = manualZoomMinimumPercentValue();
    const double maximum = manualZoomMaximumPercentValue(state);
    const double base = clampedManualZoomPercentValue(effectiveZoomPercent(state), state);
    const double targetLog
        = std::log(base) + static_cast<double>(stepCount) * std::log(manualZoomStepFactorValue());
    if (!std::isfinite(targetLog) || targetLog >= std::log(maximum)) {
        return maximum;
    }
    if (targetLog <= std::log(minimum)) {
        return minimum;
    }
    return clampedManualZoomPercentValue(std::exp(targetLog), state);
}

}
