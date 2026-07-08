#pragma once

#include "viewportcontrollercorehelpers_p.h"

#include "presentationgeometry_p.h"
#include "viewportgeometryhelpers_p.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace {

bool isPositiveGeometrySize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

QSizeF imageLogicalSize(const QImage& image)
{
    return image.isNull() ? QSizeF() : image.deviceIndependentSize();
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

enum class GeometryProjectionTarget {
    CurrentDisplay,
    PendingRender,
};

QSizeF displayedPrimaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& display = viewportDisplayState(viewport);
    if (!display.hasReadyDisplay(viewport.hasDisplayableSequence())) {
        return {};
    }
    return display.displayedImageSize;
}

QSizeF displayedSecondaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& display = viewportDisplayState(viewport);
    if (!display.hasReadyDisplay(viewport.hasDisplayableSequence())) {
        return {};
    }
    return display.secondaryDisplayedImageSize;
}

QSizeF pendingPrimaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& display = viewportDisplayState(viewport);
    if (viewport.hasProviderSequence()
        && isPositiveGeometrySize(viewportProviderState(viewport).logicalSize)) {
        return viewportProviderState(viewport).logicalSize;
    }
    const QSizeF pendingSize = imageLogicalSize(display.pendingRenderPayload.image);
    if (isPositiveGeometrySize(pendingSize)) {
        return pendingSize;
    }
    return displayedPrimaryGeometrySize(viewport);
}

QSizeF pendingSecondaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& request = viewportRequestState(viewport);
    if (!request.secondarySequence || request.secondaryActiveRequest.target.frame < 0) {
        return {};
    }

    const auto& display = viewportDisplayState(viewport);
    if (request.secondarySequenceIsProvider) {
        if (isPositiveGeometrySize(viewport.secondaryProviderState().logicalSize)) {
            return viewport.secondaryProviderState().logicalSize;
        }
        const QSizeF pendingSize = imageLogicalSize(display.secondaryPendingRenderPayload.image);
        return isPositiveGeometrySize(pendingSize) ? pendingSize
                                                   : displayedSecondaryGeometrySize(viewport);
    }

    const QSizeF pendingSize = imageLogicalSize(display.secondaryPendingRenderPayload.image);
    return isPositiveGeometrySize(pendingSize) ? pendingSize
                                               : displayedSecondaryGeometrySize(viewport);
}

QSizeF acceptedPrimaryGeometrySize(ViewportControllerPort viewport)
{
    if (viewport.hasProviderSequence()) {
        return isPositiveGeometrySize(viewportProviderState(viewport).logicalSize)
            ? viewportProviderState(viewport).logicalSize
            : QSizeF {};
    }

    const QSizeF sequenceSize = viewport.sequenceLogicalSize();
    return isPositiveGeometrySize(sequenceSize) ? sequenceSize : QSizeF {};
}

QSizeF acceptedSecondaryGeometrySize(ViewportControllerPort viewport)
{
    const auto& request = viewportRequestState(viewport);
    if (!request.secondarySequence) {
        return {};
    }
    if (request.secondarySequenceIsProvider) {
        return isPositiveGeometrySize(viewport.secondaryProviderState().logicalSize)
            ? viewport.secondaryProviderState().logicalSize
            : QSizeF {};
    }
    if (request.secondaryActiveRequest.target.frame < 0) {
        return {};
    }

    const QSizeF sequenceSize = viewport.secondarySequenceLogicalSize();
    return isPositiveGeometrySize(sequenceSize) ? sequenceSize : QSizeF {};
}

PresentationGeometry::State controllerGeometryState(ViewportControllerPort viewport,
    const ImageViewportInternal::PresentationState& presentation, double devicePixelRatio = 1.0,
    std::optional<QRectF> itemBounds = std::nullopt,
    GeometryProjectionTarget target = GeometryProjectionTarget::CurrentDisplay)
{
    const QRectF bounds = itemBounds ? *itemBounds : viewport.itemBounds();
    QSizeF primarySize = displayedPrimaryGeometrySize(viewport);
    QSizeF secondarySize = displayedSecondaryGeometrySize(viewport);
    if (target == GeometryProjectionTarget::PendingRender) {
        primarySize = pendingPrimaryGeometrySize(viewport);
        secondarySize = pendingSecondaryGeometrySize(viewport);
    }

    return viewport.engine().geometryState(
        {
            isPositiveGeometrySize(primarySize),
            bounds,
            primarySize,
            secondarySize,
            devicePixelRatio > 0.0 ? devicePixelRatio : 1.0,
        },
        presentation);
}

PresentationGeometry::State acceptedGeometryState(ViewportControllerPort viewport,
    const ImageViewportInternal::PresentationState& presentation, double devicePixelRatio = 1.0,
    std::optional<QRectF> itemBounds = std::nullopt)
{
    const QRectF bounds = itemBounds ? *itemBounds : viewport.itemBounds();
    const QSizeF primarySize = acceptedPrimaryGeometrySize(viewport);
    const QSizeF secondarySize = acceptedSecondaryGeometrySize(viewport);

    return viewport.engine().geometryState(
        {
            isPositiveGeometrySize(primarySize),
            bounds,
            primarySize,
            secondarySize,
            devicePixelRatio > 0.0 ? devicePixelRatio : 1.0,
        },
        presentation);
}

QPointF controllerContentPosition(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return PresentationGeometry::contentPosition(controllerGeometryState(viewport, presentation));
}

QPointF controllerMaximumContentPosition(
    ViewportControllerPort& viewport, const ImageViewportInternal::PresentationState& presentation)
{
    return PresentationGeometry::maximumContentPosition(
        controllerGeometryState(viewport, presentation));
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
