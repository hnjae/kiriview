#include "imageviewport_p.h"
#include "imageviewportvalidation_p.h"
#include "presentationgeometry_p.h"
#include "viewportcontrollercommandcontract_p.h"

#include <QtQuick/QQuickWindow>

#include <optional>

using namespace ImageViewportInternal;

namespace {

bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

double effectiveDevicePixelRatio(const ImageViewportPrivate& viewport)
{
    QQuickWindow* window = viewport.window();
    return window ? window->effectiveDevicePixelRatio() : 1.0;
}

PresentationGeometry::State geometryState(const ImageViewportPrivate& viewport)
{
    return viewport.controller.geometryState(effectiveDevicePixelRatio(viewport));
}

PresentationGeometry::State geometryStateForItemBounds(
    const ImageViewportPrivate& viewport, const QRectF& bounds)
{
    return viewport.controller.geometryStateForItemBounds(
        bounds, effectiveDevicePixelRatio(viewport));
}

QPointF itemCenter(const ImageViewportPrivate& viewport)
{
    return QPointF(viewport.width() / 2.0, viewport.height() / 2.0);
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

}

QRectF ImageViewportPrivate::contentRect() const
{
    return PresentationGeometry::contentRect(geometryState(*this));
}

QRectF ImageViewportPrivate::visibleImageRect() const
{
    return PresentationGeometry::visibleImageRect(geometryState(*this));
}

QRectF ImageViewportPrivate::visibleSpreadRect() const
{
    return PresentationGeometry::visibleSpreadRect(geometryState(*this));
}

QRectF ImageViewportPrivate::primaryPageRect() const
{
    return PresentationGeometry::primaryPageRect(geometryState(*this));
}

QRectF ImageViewportPrivate::secondaryPageRect() const
{
    return PresentationGeometry::secondaryPageRect(geometryState(*this));
}

QRectF ImageViewportPrivate::primaryItemRect() const
{
    return PresentationGeometry::pageItemRect(geometryState(*this), PageRole::Primary);
}

QRectF ImageViewportPrivate::secondaryItemRect() const
{
    return PresentationGeometry::pageItemRect(geometryState(*this), PageRole::Secondary);
}

QRectF ImageViewportPrivate::visiblePrimaryPageRect() const
{
    return PresentationGeometry::visiblePageRect(geometryState(*this), PageRole::Primary);
}

QRectF ImageViewportPrivate::visibleSecondaryPageRect() const
{
    return PresentationGeometry::visiblePageRect(geometryState(*this), PageRole::Secondary);
}

QSizeF ImageViewportPrivate::contentSize() const
{
    return PresentationGeometry::contentSize(geometryState(*this));
}

QPointF ImageViewportPrivate::contentPosition() const
{
    return PresentationGeometry::contentPosition(geometryState(*this));
}

QPointF ImageViewportPrivate::maximumContentPosition() const
{
    return PresentationGeometry::maximumContentPosition(geometryState(*this));
}

bool ImageViewportPrivate::horizontalPannable() const
{
    return PresentationGeometry::horizontalPannable(geometryState(*this));
}

bool ImageViewportPrivate::verticalPannable() const
{
    return PresentationGeometry::verticalPannable(geometryState(*this));
}

ImageViewportPrivate::FitMode ImageViewportPrivate::fitMode() const
{
    return lastStateSnapshot.presentation().fitMode();
}

double ImageViewportPrivate::zoomPercent() const
{
    const PresentationGeometry::State state = geometryState(*this);
    const QSizeF spreadSize = orientedSpreadSize(state);
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || !isPositiveSize(spreadSize)) {
        return lastStateSnapshot.presentation().zoomPercent();
    }

    return content.width() / spreadSize.width() * effectiveDevicePixelRatio(*this) * 100.0;
}

double ImageViewportPrivate::minimumManualZoomPercent() const
{
    return controller.minimumManualZoomPercent();
}

double ImageViewportPrivate::maximumManualZoomPercent() const
{
    return controller.maximumManualZoomPercent(effectiveDevicePixelRatio(*this));
}

double ImageViewportPrivate::manualZoomStepFactor() const
{
    return controller.manualZoomStepFactor();
}

int ImageViewportPrivate::rotationDegrees() const
{
    return lastStateSnapshot.presentation().rotationDegrees();
}

bool ImageViewportPrivate::smoothing() const
{
    return lastStateSnapshot.presentation().smoothing();
}

bool ImageViewportPrivate::mipmap() const { return lastStateSnapshot.presentation().mipmap(); }

bool ImageViewportPrivate::mirrorHorizontally() const
{
    return lastStateSnapshot.presentation().mirrorHorizontally();
}

bool ImageViewportPrivate::mirrorVertically() const
{
    return lastStateSnapshot.presentation().mirrorVertically();
}

ImageViewportPrivate::BackgroundMode ImageViewportPrivate::backgroundMode() const
{
    return lastStateSnapshot.presentation().backgroundMode();
}

QColor ImageViewportPrivate::backgroundColor() const
{
    return lastStateSnapshot.presentation().backgroundColor();
}

bool ImageViewportPrivate::looping() const { return lastStateSnapshot.presentation().looping(); }

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPresentation(
    ImageViewportPresentationCommand command)
{
    const ViewportCommandResult result = controller.setPresentation(
        { command, itemCenter(*this), effectiveDevicePixelRatio(*this) });
    applyControllerTransition(result.transition);
    return result.outcome;
}

namespace {

bool coordinateSpaceValid(ImageViewport::CoordinateSpace space)
{
    switch (space) {
    case ImageViewport::CoordinateSpace::Item:
    case ImageViewport::CoordinateSpace::DisplayedSpread:
    case ImageViewport::CoordinateSpace::DisplayedPage:
        return true;
    }
    return false;
}

bool coordinateUsesPage(const ImageViewportCoordinateInput& input)
{
    return input.sourceSpace() == ImageViewport::CoordinateSpace::DisplayedPage
        || input.targetSpace() == ImageViewport::CoordinateSpace::DisplayedPage;
}

std::optional<ImageViewport::PageRole> coordinatePageRole(const ImageViewportCoordinateInput& input)
{
    if (input.role().isNull() || !input.role().canConvert<ImageViewport::PageRole>()) {
        return std::nullopt;
    }
    const ImageViewport::PageRole role = input.role().value<ImageViewport::PageRole>();
    return ImageViewportInternal::isValidPageRole(role) ? std::optional(role) : std::nullopt;
}

bool roleIsDisplayed(ImageViewport::PageRole role, ImageViewportRoleSet displayedRoleSet)
{
    return role == ImageViewport::PageRole::Primary ? displayedRoleSet.primary()
                                                    : displayedRoleSet.secondary();
}

QVariant coordinateResultRole(
    const ImageViewportCoordinateInput& input, std::optional<ImageViewport::PageRole> role)
{
    return coordinateUsesPage(input) && role ? QVariant::fromValue(*role) : QVariant {};
}

ImageViewportCoordinateResult coordinateResultFor(const ImageViewportCoordinateInput& input,
    CoordinateResult result, std::optional<ImageViewport::PageRole> role)
{
    return ImageViewportCoordinateResult(result.isValid(), QPointF(result.x(), result.y()),
        input.targetSpace(), coordinateResultRole(input, role));
}

ImageViewportCoordinateResult invalidCoordinateResultFor(const ImageViewportCoordinateInput& input,
    std::optional<ImageViewport::PageRole> role = std::nullopt)
{
    const ImageViewport::CoordinateSpace space = coordinateSpaceValid(input.targetSpace())
        ? input.targetSpace()
        : ImageViewport::CoordinateSpace::Item;
    return ImageViewportCoordinateResult(
        false, QPointF(), space, coordinateResultRole(input, role));
}

} // namespace

ImageViewportCoordinateResult ImageViewportPrivate::mapPoint(
    const ImageViewportCoordinateInput& input) const
{
    if (!coordinateSpaceValid(input.sourceSpace()) || !coordinateSpaceValid(input.targetSpace())
        || !ImageViewportInternal::isFinitePoint(input.point())) {
        return invalidCoordinateResultFor(input);
    }
    const bool usesPage = coordinateUsesPage(input);
    const std::optional<PageRole> role = usesPage ? coordinatePageRole(input) : std::nullopt;
    if ((!usesPage && !input.role().isNull()) || (usesPage && !role)) {
        return invalidCoordinateResultFor(input);
    }
    if (usesPage && !roleIsDisplayed(*role, lastStateSnapshot.display().displayedRoleSet())) {
        return invalidCoordinateResultFor(input, role);
    }

    const QPointF point = input.point();
    const PresentationGeometry::State state = geometryState(*this);
    CoordinateResult result;
    if (input.sourceSpace() == input.targetSpace()) {
        bool valid = false;
        switch (input.sourceSpace()) {
        case ImageViewport::CoordinateSpace::Item:
            valid = PresentationGeometry::containsItemPoint(state, point.x(), point.y());
            break;
        case ImageViewport::CoordinateSpace::DisplayedSpread:
            valid = PresentationGeometry::containsSpreadPoint(state, point.x(), point.y());
            break;
        case ImageViewport::CoordinateSpace::DisplayedPage:
            valid = PresentationGeometry::containsPagePoint(state, *role, point.x(), point.y());
            break;
        }
        result = valid ? CoordinateResult(true, point.x(), point.y()) : CoordinateResult {};
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Item
        && input.targetSpace() == ImageViewport::CoordinateSpace::DisplayedSpread) {
        result = PresentationGeometry::itemToSpread(state, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::DisplayedSpread
        && input.targetSpace() == ImageViewport::CoordinateSpace::Item) {
        result = PresentationGeometry::spreadToItem(state, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::Item
        && input.targetSpace() == ImageViewport::CoordinateSpace::DisplayedPage) {
        result = PresentationGeometry::itemToPage(state, *role, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::DisplayedPage
        && input.targetSpace() == ImageViewport::CoordinateSpace::Item) {
        result = PresentationGeometry::pageToItem(state, *role, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::DisplayedSpread
        && input.targetSpace() == ImageViewport::CoordinateSpace::DisplayedPage) {
        result = PresentationGeometry::spreadToPage(state, *role, point.x(), point.y());
    } else if (input.sourceSpace() == ImageViewport::CoordinateSpace::DisplayedPage
        && input.targetSpace() == ImageViewport::CoordinateSpace::DisplayedSpread) {
        result = PresentationGeometry::pageToSpread(state, *role, point.x(), point.y());
    }

    return coordinateResultFor(input, result, role);
}

bool ImageViewportPrivate::containsPoint(const ImageViewportCoordinateInput& input) const
{
    return mapPoint(input).isValid();
}

QRectF ImageViewportPrivate::currentContentRect() const
{
    return PresentationGeometry::contentRect(geometryState(*this));
}

QRectF ImageViewportPrivate::itemBounds() const
{
    if (width() <= 0.0 || height() <= 0.0) {
        return {};
    }

    return QRectF(0.0, 0.0, width(), height());
}

QRectF ImageViewportPrivate::contentRectForItemBounds(const QRectF& bounds) const
{
    return PresentationGeometry::contentRect(geometryStateForItemBounds(*this, bounds));
}

QRectF ImageViewportPrivate::visibleImageRectForItemBounds(const QRectF& bounds) const
{
    return PresentationGeometry::visibleImageRect(geometryStateForItemBounds(*this, bounds));
}
