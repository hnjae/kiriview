#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <algorithm>
#include <cmath>

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

QSizeF secondaryImageSize(const ImageViewportPrivate& viewport)
{
    return viewport.secondaryLogicalSize();
}

PresentationGeometry::State geometryState(const ImageViewportPrivate& viewport)
{
    return {
        viewport.hasReadyDisplay(),
        viewport.itemBounds(),
        viewport.currentImageSize(),
        secondaryImageSize(viewport),
        viewport.presentation.pageGap,
        viewport.presentation.spreadDirection,
        viewport.presentation.fitMode,
        viewport.presentation.fillMode,
        viewport.presentation.horizontalAlignment,
        viewport.presentation.verticalAlignment,
        viewport.presentation.rotationDegrees,
        viewport.presentation.mirrorHorizontally,
        viewport.presentation.mirrorVertically,
        viewport.presentation.zoom,
        effectiveDevicePixelRatio(viewport),
        viewport.presentation.pan,
    };
}

PresentationGeometry::State geometryStateForItemBounds(
    const ImageViewportPrivate& viewport, const QRectF& bounds)
{
    PresentationGeometry::State state = geometryState(viewport);
    state.itemBounds = bounds;
    return state;
}

PresentationGeometry::State geometryStateForImageSize(
    const ImageViewportPrivate& viewport, QSizeF imageSize)
{
    PresentationGeometry::State state = geometryState(viewport);
    state.hasReadyDisplay = !imageSize.isEmpty();
    state.primaryImageSize = imageSize;
    state.secondaryImageSize = {};
    return state;
}

QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

QPointF contentPositionForRect(const QRectF& contentRect, const QRectF& itemBounds)
{
    if (contentRect.isEmpty() || itemBounds.isEmpty()) {
        return {};
    }

    const QPointF maximum(std::max(0.0, contentRect.width() - itemBounds.width()),
        std::max(0.0, contentRect.height() - itemBounds.height()));
    return clampedPoint(QPointF(-contentRect.x(), -contentRect.y()), {}, maximum);
}

QPointF maximumContentPositionForRect(const QRectF& contentRect, const QRectF& itemBounds)
{
    if (contentRect.isEmpty() || itemBounds.isEmpty()) {
        return {};
    }

    return QPointF(std::max(0.0, contentRect.width() - itemBounds.width()),
        std::max(0.0, contentRect.height() - itemBounds.height()));
}

bool applyContentPosition(ImageViewportPrivate& viewport, QPointF requestedPosition)
{
    const QRectF content = viewport.contentRect();
    const QRectF bounds = viewport.itemBounds();
    if (content.isEmpty() || bounds.isEmpty()) {
        return false;
    }

    const QPointF currentPosition = contentPositionForRect(content, bounds);
    const QPointF maximum = maximumContentPositionForRect(content, bounds);
    const QPointF nextPosition = clampedPoint(requestedPosition, {}, maximum);
    if (nextPosition == currentPosition) {
        return false;
    }

    viewport.presentation.pan += currentPosition - nextPosition;
    return true;
}

bool clampPresentationPanToBounds(ImageViewportPrivate& viewport)
{
    const QPointF savedPan = viewport.presentation.pan;
    const QRectF currentContent = viewport.contentRect();
    const QRectF bounds = viewport.itemBounds();
    if (currentContent.isEmpty() || bounds.isEmpty()) {
        return false;
    }

    viewport.presentation.pan = {};
    const QRectF baseContent = viewport.contentRect();
    viewport.presentation.pan = savedPan;

    const QPointF maximum = maximumContentPositionForRect(currentContent, bounds);
    const QPointF currentPosition = contentPositionForRect(currentContent, bounds);
    const QPointF clampedPosition = clampedPoint(currentPosition, {}, maximum);
    QPointF targetTopLeft = currentContent.topLeft();
    targetTopLeft.setX(maximum.x() == 0.0 ? baseContent.x() : -clampedPosition.x());
    targetTopLeft.setY(maximum.y() == 0.0 ? baseContent.y() : -clampedPosition.y());

    const QPointF adjustment = targetTopLeft - currentContent.topLeft();
    if (adjustment.isNull()) {
        return false;
    }

    viewport.presentation.pan += adjustment;
    return true;
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

QSizeF ImageViewportPrivate::contentSize() const { return contentRect().size(); }

QPointF ImageViewportPrivate::contentPosition() const
{
    return contentPositionForRect(contentRect(), itemBounds());
}

QPointF ImageViewportPrivate::maximumContentPosition() const
{
    return maximumContentPositionForRect(contentRect(), itemBounds());
}

bool ImageViewportPrivate::horizontalPannable() const { return maximumContentPosition().x() > 0.0; }

bool ImageViewportPrivate::verticalPannable() const { return maximumContentPosition().y() > 0.0; }

QRectF ImageViewportPrivate::contentRectForImageSize(QSizeF imageSize) const
{
    return PresentationGeometry::contentRect(geometryStateForImageSize(*this, imageSize));
}

QRectF ImageViewportPrivate::visibleImageRectForImageSize(QSizeF imageSize) const
{
    return PresentationGeometry::visibleImageRect(geometryStateForImageSize(*this, imageSize));
}

ImageViewportPrivate::FitMode ImageViewportPrivate::fitMode() const { return presentation.fitMode; }

void ImageViewportPrivate::setFitModeProperty(FitMode mode) { setFitMode(mode, {}); }

double ImageViewportPrivate::zoomPercent() const
{
    const PresentationGeometry::State state = geometryState(*this);
    const QSizeF spreadSize = orientedSpreadSize(state);
    const QRectF content = PresentationGeometry::contentRect(state);
    if (content.isEmpty() || !isPositiveSize(spreadSize)) {
        return presentation.zoom * 100.0;
    }

    return content.width() / spreadSize.width() * effectiveDevicePixelRatio(*this) * 100.0;
}

void ImageViewportPrivate::setZoomPercentProperty(double percent) { setZoomPercent(percent, {}); }

int ImageViewportPrivate::rotationDegrees() const { return presentation.rotationDegrees; }

ImageViewportPrivate::FillMode ImageViewportPrivate::fillMode() const
{
    return presentation.fillMode;
}

void ImageViewportPrivate::setFillMode(FillMode mode)
{
    if (!isValidFillMode(mode) || presentation.fillMode == mode) {
        return;
    }

    presentation.fillMode = mode;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::HorizontalAlignment ImageViewportPrivate::horizontalAlignment() const
{
    return presentation.horizontalAlignment;
}

void ImageViewportPrivate::setHorizontalAlignment(HorizontalAlignment alignment)
{
    if (!isValidHorizontalAlignment(alignment) || presentation.horizontalAlignment == alignment) {
        return;
    }

    presentation.horizontalAlignment = alignment;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::VerticalAlignment ImageViewportPrivate::verticalAlignment() const
{
    return presentation.verticalAlignment;
}

void ImageViewportPrivate::setVerticalAlignment(VerticalAlignment alignment)
{
    if (!isValidVerticalAlignment(alignment) || presentation.verticalAlignment == alignment) {
        return;
    }

    presentation.verticalAlignment = alignment;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::smoothing() const { return presentation.smoothing; }

void ImageViewportPrivate::setSmoothing(bool smoothing)
{
    if (presentation.smoothing == smoothing) {
        return;
    }

    presentation.smoothing = smoothing;
    notifyPresentationChanged(false);
}

bool ImageViewportPrivate::mipmap() const { return presentation.mipmap; }

void ImageViewportPrivate::setMipmap(bool mipmap)
{
    if (presentation.mipmap == mipmap) {
        return;
    }

    presentation.mipmap = mipmap;
    notifyPresentationChanged(false);
}

bool ImageViewportPrivate::mirrorHorizontally() const { return presentation.mirrorHorizontally; }

void ImageViewportPrivate::setMirrorHorizontally(bool mirror)
{
    if (presentation.mirrorHorizontally == mirror) {
        return;
    }

    presentation.mirrorHorizontally = mirror;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::mirrorVertically() const { return presentation.mirrorVertically; }

void ImageViewportPrivate::setMirrorVertically(bool mirror)
{
    if (presentation.mirrorVertically == mirror) {
        return;
    }

    presentation.mirrorVertically = mirror;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::BackgroundMode ImageViewportPrivate::backgroundMode() const
{
    return presentation.backgroundMode;
}

void ImageViewportPrivate::setBackgroundMode(BackgroundMode mode)
{
    if (!isValidBackgroundMode(mode) || presentation.backgroundMode == mode) {
        return;
    }

    presentation.backgroundMode = mode;
    notifyPresentationChanged(false);
}

QColor ImageViewportPrivate::backgroundColor() const { return presentation.backgroundColor; }

void ImageViewportPrivate::setBackgroundColor(const QColor& color)
{
    if (presentation.backgroundColor == color) {
        return;
    }

    presentation.backgroundColor = color;
    notifyPresentationChanged(false);
}

double ImageViewportPrivate::zoom() const { return presentation.zoom; }

void ImageViewportPrivate::setZoom(double zoom)
{
    if (!isFinitePositive(zoom) || presentation.zoom == zoom) {
        return;
    }

    presentation.zoom = zoom;
    notifyPresentationChanged(true);
}

QPointF ImageViewportPrivate::pan() const { return presentation.pan; }

void ImageViewportPrivate::setPan(QPointF pan)
{
    const bool unchanged = presentation.pan.x() == pan.x() && presentation.pan.y() == pan.y();
    if (!isFinitePoint(pan) || unchanged) {
        return;
    }

    presentation.pan = pan;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::looping() const { return controller.looping(); }

void ImageViewportPrivate::setLooping(bool looping)
{
    applyControllerChanges(controller.setLooping(looping));
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setSpreadDirection(
    SpreadDirection direction)
{
    if (!isValidSpreadDirection(direction)) {
        return CommandOutcome::Invalid;
    }
    if (presentation.spreadDirection == direction) {
        return CommandOutcome::Accepted;
    }

    presentation.spreadDirection = direction;
    clampPresentationPanToBounds(*this);
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setPageGap(double gap)
{
    if (!std::isfinite(gap) || gap < 0.0) {
        return CommandOutcome::Invalid;
    }
    if (presentation.pageGap == gap) {
        return CommandOutcome::Accepted;
    }

    presentation.pageGap = gap;
    clampPresentationPanToBounds(*this);
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setFitMode(FitMode mode, QPointF anchor)
{
    if (!isValidFitMode(mode) || !isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }
    if (presentation.fitMode == mode) {
        return CommandOutcome::Accepted;
    }

    presentation.fitMode = mode;
    clampPresentationPanToBounds(*this);
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setZoomPercent(
    double percent, QPointF anchor)
{
    if (!isFinitePositive(percent) || percent > ImageViewportDisplayLimits::maximumManualZoomPercent()
        || !isFinitePoint(anchor)) {
        const ViewportCommandResult result = controller.rejectInvalidCommand();
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    const double zoom = percent / 100.0;
    if (presentation.fitMode == FitMode::Manual && presentation.zoom == zoom) {
        return CommandOutcome::Accepted;
    }

    presentation.fitMode = FitMode::Manual;
    presentation.zoom = zoom;
    clampPresentationPanToBounds(*this);
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panBy(QPointF delta)
{
    if (!isFinitePoint(delta)) {
        return CommandOutcome::Invalid;
    }
    if (delta.isNull()) {
        return CommandOutcome::Accepted;
    }

    if (applyContentPosition(*this, contentPosition() + delta)) {
        notifyPresentationChanged(true);
    }
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToStart()
{
    if (applyContentPosition(*this, {})) {
        notifyPresentationChanged(true);
    }
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::panToEnd()
{
    if (applyContentPosition(*this, maximumContentPosition())) {
        notifyPresentationChanged(true);
    }
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::scanNext()
{
    const QPointF maximum = maximumContentPosition();
    if (maximum.y() > 0.0) {
        return panBy(QPointF(0.0, std::max(1.0, itemBounds().height() * 0.9)));
    }
    if (maximum.x() > 0.0) {
        return panBy(QPointF(std::max(1.0, itemBounds().width() * 0.9), 0.0));
    }
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::scanPrevious()
{
    const QPointF maximum = maximumContentPosition();
    if (maximum.y() > 0.0) {
        return panBy(QPointF(0.0, -std::max(1.0, itemBounds().height() * 0.9)));
    }
    if (maximum.x() > 0.0) {
        return panBy(QPointF(-std::max(1.0, itemBounds().width() * 0.9), 0.0));
    }
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::rotateClockwise(QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    presentation.rotationDegrees = (presentation.rotationDegrees + 90) % 360;
    clampPresentationPanToBounds(*this);
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::rotateCounterClockwise(QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    presentation.rotationDegrees = (presentation.rotationDegrees + 270) % 360;
    clampPresentationPanToBounds(*this);
    notifyPresentationChanged(true);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setMirrorHorizontally(
    bool enabled, QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    setMirrorHorizontally(enabled);
    return CommandOutcome::Accepted;
}

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::setMirrorVertically(
    bool enabled, QPointF anchor)
{
    if (!isFinitePoint(anchor)) {
        return CommandOutcome::Invalid;
    }

    setMirrorVertically(enabled);
    return CommandOutcome::Accepted;
}

QVariantMap ImageViewportPrivate::itemToSpread(double x, double y) const
{
    return PresentationGeometry::itemToSpread(geometryState(*this), x, y);
}

QVariantMap ImageViewportPrivate::spreadToItem(double x, double y) const
{
    return PresentationGeometry::spreadToItem(geometryState(*this), x, y);
}

QVariantMap ImageViewportPrivate::itemToPage(PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return invalidCoordinateResult();
    }

    return PresentationGeometry::itemToPage(geometryState(*this), role, x, y);
}

QVariantMap ImageViewportPrivate::pageToItem(PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return invalidCoordinateResult();
    }

    return PresentationGeometry::pageToItem(geometryState(*this), role, x, y);
}

bool ImageViewportPrivate::containsVisibleSpreadPoint(double x, double y) const
{
    return PresentationGeometry::containsVisibleSpreadPoint(geometryState(*this), x, y);
}

bool ImageViewportPrivate::containsVisiblePagePoint(PageRole role, double x, double y) const
{
    if (!isValidPageRole(role)) {
        return false;
    }

    return PresentationGeometry::containsVisiblePagePoint(geometryState(*this), role, x, y);
}

QVariantMap ImageViewportPrivate::itemToImage(double x, double y) const
{
    return PresentationGeometry::itemToImage(geometryState(*this), x, y);
}

QVariantMap ImageViewportPrivate::imageToItem(double x, double y) const
{
    return PresentationGeometry::imageToItem(geometryState(*this), x, y);
}

bool ImageViewportPrivate::containsVisibleImagePoint(double x, double y) const
{
    return PresentationGeometry::containsVisibleImagePoint(geometryState(*this), x, y);
}

QVariantMap ImageViewportPrivate::invalidRange()
{
    return {
        { QStringLiteral("minimum"), -1 },
        { QStringLiteral("maximum"), -1 },
    };
}

QVariantMap ImageViewportPrivate::invalidCoordinateResult()
{
    return PresentationGeometry::invalidCoordinateResult();
}

void ImageViewportPrivate::notifyPresentationChanged(bool affectsGeometry)
{
    incrementDisplayRevision();
    emit q->presentationChanged();
    if (affectsGeometry && hasReadyDisplay() && !itemBounds().isEmpty()) {
        emit q->geometryStateChanged();
    }
    update();
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

QSizeF ImageViewportPrivate::currentImageSize() const
{
    if (!hasReadyDisplay()) {
        return {};
    }

    return controller.displayState().displayedImageSize;
}
