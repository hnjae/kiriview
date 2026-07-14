#include "presentationgeometry_p.h"

#include <algorithm>
#include <cmath>

namespace {

bool isPositiveSize(QSizeF size)
{
    return size.isValid() && size.width() > 0.0 && size.height() > 0.0;
}

bool containsHalfOpen(const QRectF& rect, QPointF point)
{
    return !rect.isEmpty() && point.x() >= rect.left() && point.y() >= rect.top()
        && point.x() < rect.right() && point.y() < rect.bottom();
}

QPointF clampedPoint(QPointF point, QPointF minimum, QPointF maximum)
{
    return QPointF(std::clamp(point.x(), minimum.x(), maximum.x()),
        std::clamp(point.y(), minimum.y(), maximum.y()));
}

CoordinateResult coordinateResult(QPointF point)
{
    return CoordinateResult(true, point.x(), point.y());
}

int normalizedRotation(int degrees)
{
    int normalized = degrees % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    return normalized;
}

QSizeF rotatedSize(QSizeF size, int rotationDegrees)
{
    const int rotation = normalizedRotation(rotationDegrees);
    if (rotation == 90 || rotation == 270) {
        return QSizeF(size.height(), size.width());
    }
    return size;
}

QSizeF spreadSizeForState(const PresentationGeometry::State& state)
{
    if (!isPositiveSize(state.primaryImageSize)) {
        return {};
    }
    if (!isPositiveSize(state.secondaryImageSize)) {
        return state.primaryImageSize;
    }

    return QSizeF(state.primaryImageSize.width() + state.pageGap + state.secondaryImageSize.width(),
        std::max(state.primaryImageSize.height(), state.secondaryImageSize.height()));
}

QRectF primaryPageRectForState(const PresentationGeometry::State& state)
{
    if (!isPositiveSize(state.primaryImageSize)) {
        return {};
    }
    if (!isPositiveSize(state.secondaryImageSize)
        || state.spreadDirection == ImageViewport::SpreadDirection::LeftToRight) {
        return QRectF(QPointF(0.0, 0.0), state.primaryImageSize);
    }

    return QRectF(
        QPointF(state.secondaryImageSize.width() + state.pageGap, 0.0), state.primaryImageSize);
}

QRectF secondaryPageRectForState(const PresentationGeometry::State& state)
{
    if (!isPositiveSize(state.primaryImageSize) || !isPositiveSize(state.secondaryImageSize)) {
        return {};
    }
    if (state.spreadDirection == ImageViewport::SpreadDirection::RightToLeft) {
        return QRectF(QPointF(0.0, 0.0), state.secondaryImageSize);
    }

    return QRectF(
        QPointF(state.primaryImageSize.width() + state.pageGap, 0.0), state.secondaryImageSize);
}

QRectF pageRectForRole(const PresentationGeometry::State& state, ImageViewport::PageRole role)
{
    switch (role) {
    case ImageViewport::PageRole::Primary:
        return primaryPageRectForState(state);
    case ImageViewport::PageRole::Secondary:
        return secondaryPageRectForState(state);
    }
    return {};
}

bool hasPresentableGeometry(const PresentationGeometry::State& state)
{
    return state.hasReadyDisplay && !state.itemBounds.isEmpty()
        && isPositiveSize(spreadSizeForState(state));
}

QPointF mirrorSpreadPoint(const PresentationGeometry::State& state, QPointF point)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    if (state.mirrorHorizontally) {
        point.setX(spreadSize.width() - point.x());
    }
    if (state.mirrorVertically) {
        point.setY(spreadSize.height() - point.y());
    }
    return point;
}

QPointF unmirrorSpreadPoint(const PresentationGeometry::State& state, QPointF point)
{
    return mirrorSpreadPoint(state, point);
}

QPointF rotateSpreadPointToOriented(const PresentationGeometry::State& state, QPointF point)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    switch (normalizedRotation(state.rotationDegrees)) {
    case 90:
        return QPointF(spreadSize.height() - point.y(), point.x());
    case 180:
        return QPointF(spreadSize.width() - point.x(), spreadSize.height() - point.y());
    case 270:
        return QPointF(point.y(), spreadSize.width() - point.x());
    case 0:
    default:
        return point;
    }
}

QPointF unrotateOrientedPointToSpread(const PresentationGeometry::State& state, QPointF point)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    switch (normalizedRotation(state.rotationDegrees)) {
    case 90:
        return QPointF(point.y(), spreadSize.height() - point.x());
    case 180:
        return QPointF(spreadSize.width() - point.x(), spreadSize.height() - point.y());
    case 270:
        return QPointF(spreadSize.width() - point.y(), point.x());
    case 0:
    default:
        return point;
    }
}

QPointF spreadToOrientedPoint(const PresentationGeometry::State& state, QPointF point)
{
    return rotateSpreadPointToOriented(state, mirrorSpreadPoint(state, point));
}

QPointF orientedToSpreadPoint(const PresentationGeometry::State& state, QPointF point)
{
    return unmirrorSpreadPoint(state, unrotateOrientedPointToSpread(state, point));
}

QRectF spreadRectToOrientedRect(const PresentationGeometry::State& state, const QRectF& rect)
{
    if (rect.isEmpty()) {
        return {};
    }

    const QPointF points[] = {
        spreadToOrientedPoint(state, rect.topLeft()),
        spreadToOrientedPoint(state, rect.topRight()),
        spreadToOrientedPoint(state, rect.bottomLeft()),
        spreadToOrientedPoint(state, rect.bottomRight()),
    };

    double left = points[0].x();
    double right = points[0].x();
    double top = points[0].y();
    double bottom = points[0].y();
    for (const QPointF point : points) {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

QRectF orientedRectToSpreadRect(const PresentationGeometry::State& state, const QRectF& rect)
{
    if (rect.isEmpty()) {
        return {};
    }

    const QPointF points[] = {
        orientedToSpreadPoint(state, rect.topLeft()),
        orientedToSpreadPoint(state, rect.topRight()),
        orientedToSpreadPoint(state, rect.bottomLeft()),
        orientedToSpreadPoint(state, rect.bottomRight()),
    };

    double left = points[0].x();
    double right = points[0].x();
    double top = points[0].y();
    double bottom = points[0].y();
    for (const QPointF point : points) {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

QSizeF placedContentSizeForReadyState(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    if (!state.hasReadyDisplay || state.itemBounds.isEmpty() || !isPositiveSize(spreadSize)) {
        return {};
    }

    const QSizeF fittingSize = rotatedSize(spreadSize, state.rotationDegrees);
    if (state.fitMode == ImageViewport::FitMode::FitWidth) {
        const double scale = state.itemBounds.width() / fittingSize.width();
        return fittingSize * scale;
    }
    if (state.fitMode == ImageViewport::FitMode::FitHeight) {
        const double scale = state.itemBounds.height() / fittingSize.height();
        return fittingSize * scale;
    }
    if (state.fitMode == ImageViewport::FitMode::Manual) {
        const double devicePixelRatio = state.devicePixelRatio > 0.0 ? state.devicePixelRatio : 1.0;
        return fittingSize * (state.manualZoom / devicePixelRatio);
    }

    const double scale = std::min(state.itemBounds.width() / fittingSize.width(),
        state.itemBounds.height() / fittingSize.height());
    return fittingSize * scale;
}

QPointF maximumContentPositionForPlacedSize(
    const PresentationGeometry::State& state, QSizeF placedSize)
{
    if (placedSize.isEmpty() || state.itemBounds.isEmpty()) {
        return {};
    }

    return QPointF(std::max(0.0, placedSize.width() - state.itemBounds.width()),
        std::max(0.0, placedSize.height() - state.itemBounds.height()));
}

QRectF contentRectForReadyState(const PresentationGeometry::State& state)
{
    const QSizeF placedSize = placedContentSizeForReadyState(state);
    if (!isPositiveSize(placedSize)) {
        return {};
    }

    const QPointF maximum = maximumContentPositionForPlacedSize(state, placedSize);
    const QPointF position = clampedPoint(state.contentPosition, {}, maximum);
    const double centeredX = (state.itemBounds.width() - placedSize.width()) / 2.0;
    const double centeredY = (state.itemBounds.height() - placedSize.height()) / 2.0;
    const double x = maximum.x() == 0.0 ? centeredX : -position.x();
    const double y = maximum.y() == 0.0 ? centeredY : -position.y();
    return QRectF(x, y, placedSize.width(), placedSize.height());
}

QRectF visibleItemRectForState(const PresentationGeometry::State& state)
{
    const QRectF content = contentRectForReadyState(state);
    if (content.isEmpty()) {
        return {};
    }
    return content.intersected(state.itemBounds);
}

QPointF maximumContentPositionForState(const PresentationGeometry::State& state)
{
    return maximumContentPositionForPlacedSize(state, placedContentSizeForReadyState(state));
}

QPointF contentPositionForState(const PresentationGeometry::State& state)
{
    const QSizeF placedSize = placedContentSizeForReadyState(state);
    if (!isPositiveSize(placedSize) || state.itemBounds.isEmpty()) {
        return {};
    }

    return clampedPoint(state.contentPosition, {},
        maximumContentPositionForPlacedSize(state, placedSize));
}

QPointF contentPositionForAnchoredSpreadPointForState(
    const PresentationGeometry::State& state, QPointF spreadPoint, QPointF itemPoint)
{
    const QSizeF placedSize = placedContentSizeForReadyState(state);
    const QSizeF fittingSize = rotatedSize(spreadSizeForState(state), state.rotationDegrees);
    if (!isPositiveSize(placedSize) || !isPositiveSize(fittingSize)
        || !std::isfinite(spreadPoint.x()) || !std::isfinite(spreadPoint.y())
        || !std::isfinite(itemPoint.x()) || !std::isfinite(itemPoint.y())) {
        return contentPositionForState(state);
    }

    const QPointF orientedPoint = spreadToOrientedPoint(state, spreadPoint);
    const QPointF topLeft(
        itemPoint.x() - orientedPoint.x() / fittingSize.width() * placedSize.width(),
        itemPoint.y() - orientedPoint.y() / fittingSize.height() * placedSize.height());
    const QPointF maximum = maximumContentPositionForPlacedSize(state, placedSize);
    return clampedPoint(QPointF(maximum.x() == 0.0 ? 0.0 : -topLeft.x(),
                            maximum.y() == 0.0 ? 0.0 : -topLeft.y()),
        {}, maximum);
}

QPointF itemToOrientedPoint(
    const PresentationGeometry::State& state, const QRectF& content, QPointF point)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    const QSizeF fittingSize = rotatedSize(spreadSize, state.rotationDegrees);
    return QPointF((point.x() - content.x()) / content.width() * fittingSize.width(),
        (point.y() - content.y()) / content.height() * fittingSize.height());
}

QPointF orientedToItemPoint(
    const PresentationGeometry::State& state, const QRectF& content, QPointF point)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    const QSizeF fittingSize = rotatedSize(spreadSize, state.rotationDegrees);
    return QPointF(content.x() + point.x() / fittingSize.width() * content.width(),
        content.y() + point.y() / fittingSize.height() * content.height());
}

QRectF itemRectForSpreadRect(const PresentationGeometry::State& state, const QRectF& spreadRect)
{
    const QRectF content = contentRectForReadyState(state);
    const QSizeF fittingSize = rotatedSize(spreadSizeForState(state), state.rotationDegrees);
    if (content.isEmpty() || spreadRect.isEmpty() || !isPositiveSize(fittingSize)) {
        return {};
    }

    const QRectF orientedRect = spreadRectToOrientedRect(state, spreadRect);
    return QRectF(content.x() + orientedRect.x() / fittingSize.width() * content.width(),
        content.y() + orientedRect.y() / fittingSize.height() * content.height(),
        orientedRect.width() / fittingSize.width() * content.width(),
        orientedRect.height() / fittingSize.height() * content.height());
}

QRectF visibleSpreadRectForState(const PresentationGeometry::State& state)
{
    if (!hasPresentableGeometry(state)) {
        return {};
    }

    const QRectF content = contentRectForReadyState(state);
    const QRectF visibleItemRect = visibleItemRectForState(state);
    if (content.isEmpty() || visibleItemRect.isEmpty()) {
        return {};
    }

    const QPointF orientedTopLeft = itemToOrientedPoint(state, content, visibleItemRect.topLeft());
    const QPointF orientedBottomRight
        = itemToOrientedPoint(state, content, visibleItemRect.bottomRight());
    return orientedRectToSpreadRect(state, QRectF(orientedTopLeft, orientedBottomRight));
}

QRectF visiblePageRectForState(
    const PresentationGeometry::State& state, ImageViewport::PageRole role)
{
    const QRectF pageRect = pageRectForRole(state, role);
    const QRectF visibleSpreadRect = visibleSpreadRectForState(state);
    if (pageRect.isEmpty() || visibleSpreadRect.isEmpty()) {
        return {};
    }

    QRectF visiblePageRect = pageRect.intersected(visibleSpreadRect);
    if (visiblePageRect.isEmpty()) {
        return {};
    }
    visiblePageRect.translate(-pageRect.topLeft());
    return visiblePageRect;
}

}

QSizeF PresentationGeometry::spreadSize(const State& state) { return spreadSizeForState(state); }

QRectF PresentationGeometry::primaryPageRect(const State& state)
{
    return primaryPageRectForState(state);
}

QRectF PresentationGeometry::secondaryPageRect(const State& state)
{
    return secondaryPageRectForState(state);
}

QRectF PresentationGeometry::contentRect(const State& state)
{
    return contentRectForReadyState(state);
}

QSizeF PresentationGeometry::contentSize(const State& state)
{
    return contentRectForReadyState(state).size();
}

QPointF PresentationGeometry::contentPosition(const State& state)
{
    return contentPositionForState(state);
}

QPointF PresentationGeometry::contentPositionForAnchoredSpreadPoint(
    const State& state, QPointF spreadPoint, QPointF itemPoint)
{
    return contentPositionForAnchoredSpreadPointForState(state, spreadPoint, itemPoint);
}

QPointF PresentationGeometry::maximumContentPosition(const State& state)
{
    return maximumContentPositionForState(state);
}

bool PresentationGeometry::horizontalPannable(const State& state)
{
    return maximumContentPositionForState(state).x() > 0.0;
}

bool PresentationGeometry::verticalPannable(const State& state)
{
    return maximumContentPositionForState(state).y() > 0.0;
}

QRectF PresentationGeometry::visibleImageRect(const State& state)
{
    return visiblePageRectForState(state, ImageViewport::PageRole::Primary);
}

QRectF PresentationGeometry::visibleSpreadRect(const State& state)
{
    return visibleSpreadRectForState(state);
}

QRectF PresentationGeometry::visiblePageRect(const State& state, ImageViewport::PageRole role)
{
    return visiblePageRectForState(state, role);
}

QRectF PresentationGeometry::pageItemRect(const State& state, ImageViewport::PageRole role)
{
    return itemRectForSpreadRect(state, pageRectForRole(state, role));
}

CoordinateResult PresentationGeometry::itemToSpread(const State& state, double x, double y)
{
    if (!hasPresentableGeometry(state) || !std::isfinite(x) || !std::isfinite(y)) {
        return invalidCoordinateResult();
    }

    const QRectF content = contentRectForReadyState(state);
    const QRectF visibleItemRect = visibleItemRectForState(state);
    if (content.isEmpty() || visibleItemRect.isEmpty()
        || !containsHalfOpen(visibleItemRect, QPointF(x, y))) {
        return invalidCoordinateResult();
    }

    const QPointF orientedPoint = itemToOrientedPoint(state, content, QPointF(x, y));
    const QPointF spreadPoint = orientedToSpreadPoint(state, orientedPoint);
    if (!containsVisibleSpreadPoint(state, spreadPoint.x(), spreadPoint.y())) {
        return invalidCoordinateResult();
    }
    return coordinateResult(spreadPoint);
}

CoordinateResult PresentationGeometry::spreadToItem(const State& state, double x, double y)
{
    if (!containsVisibleSpreadPoint(state, x, y)) {
        return invalidCoordinateResult();
    }

    const QRectF content = contentRectForReadyState(state);
    const QPointF itemPoint
        = orientedToItemPoint(state, content, spreadToOrientedPoint(state, QPointF(x, y)));
    if (!containsHalfOpen(state.itemBounds, itemPoint)) {
        return invalidCoordinateResult();
    }
    return coordinateResult(itemPoint);
}

CoordinateResult PresentationGeometry::itemToPage(
    const State& state, ImageViewport::PageRole role, double x, double y)
{
    const CoordinateResult spreadPoint = itemToSpread(state, x, y);
    if (!spreadPoint.isValid()) {
        return invalidCoordinateResult();
    }

    const QRectF pageRect = pageRectForRole(state, role);
    const QPointF point(spreadPoint.x(), spreadPoint.y());
    if (!containsHalfOpen(pageRect, point)) {
        return invalidCoordinateResult();
    }

    return coordinateResult(point - pageRect.topLeft());
}

CoordinateResult PresentationGeometry::pageToItem(
    const State& state, ImageViewport::PageRole role, double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return invalidCoordinateResult();
    }

    const QRectF pageRect = pageRectForRole(state, role);
    const QRectF pageLocalRect(QPointF(0.0, 0.0), pageRect.size());
    if (!containsHalfOpen(pageLocalRect, QPointF(x, y))) {
        return invalidCoordinateResult();
    }

    return spreadToItem(state, pageRect.x() + x, pageRect.y() + y);
}

CoordinateResult PresentationGeometry::spreadToPage(
    const State& state, ImageViewport::PageRole role, double x, double y)
{
    if (!containsSpreadPoint(state, x, y)) {
        return invalidCoordinateResult();
    }

    const QRectF pageRect = pageRectForRole(state, role);
    const QPointF point(x, y);
    if (!containsHalfOpen(pageRect, point)) {
        return invalidCoordinateResult();
    }
    return coordinateResult(point - pageRect.topLeft());
}

CoordinateResult PresentationGeometry::pageToSpread(
    const State& state, ImageViewport::PageRole role, double x, double y)
{
    if (!containsPagePoint(state, role, x, y)) {
        return invalidCoordinateResult();
    }

    const QRectF pageRect = pageRectForRole(state, role);
    return coordinateResult(pageRect.topLeft() + QPointF(x, y));
}

bool PresentationGeometry::containsItemPoint(const State& state, double x, double y)
{
    return hasPresentableGeometry(state) && std::isfinite(x) && std::isfinite(y)
        && containsHalfOpen(state.itemBounds, QPointF(x, y));
}

bool PresentationGeometry::containsSpreadPoint(const State& state, double x, double y)
{
    if (!hasPresentableGeometry(state) || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    return containsHalfOpen(QRectF(QPointF(0.0, 0.0), spreadSizeForState(state)), QPointF(x, y));
}

bool PresentationGeometry::containsPagePoint(
    const State& state, ImageViewport::PageRole role, double x, double y)
{
    if (!hasPresentableGeometry(state) || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    const QRectF pageRect = pageRectForRole(state, role);
    return containsHalfOpen(QRectF(QPointF(0.0, 0.0), pageRect.size()), QPointF(x, y));
}

bool PresentationGeometry::containsVisibleSpreadPoint(const State& state, double x, double y)
{
    if (!hasPresentableGeometry(state) || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    const QRectF spreadRect(QPointF(0.0, 0.0), spreadSizeForState(state));
    if (!containsHalfOpen(spreadRect, QPointF(x, y))) {
        return false;
    }

    return containsHalfOpen(visibleSpreadRectForState(state), QPointF(x, y));
}

CoordinateResult PresentationGeometry::invalidCoordinateResult() { return {}; }
