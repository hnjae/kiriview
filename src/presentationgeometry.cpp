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

QRectF contentRectForReadyState(const PresentationGeometry::State& state)
{
    const QSizeF spreadSize = spreadSizeForState(state);
    if (!state.hasReadyDisplay || state.itemBounds.isEmpty() || !isPositiveSize(spreadSize)) {
        return {};
    }

    const QSizeF fittingSize = rotatedSize(spreadSize, state.rotationDegrees);
    QSizeF placedSize;
    if (state.fitMode == ImageViewport::FitMode::FitWidth) {
        const double scale = state.itemBounds.width() / fittingSize.width();
        placedSize = fittingSize * scale;
    } else if (state.fitMode == ImageViewport::FitMode::FitHeight) {
        const double scale = state.itemBounds.height() / fittingSize.height();
        placedSize = fittingSize * scale;
    } else if (state.fitMode == ImageViewport::FitMode::Manual) {
        const double devicePixelRatio = state.devicePixelRatio > 0.0 ? state.devicePixelRatio : 1.0;
        placedSize = fittingSize * (state.zoom / devicePixelRatio);
    } else {
        switch (state.fillMode) {
        case ImageViewportInternal::ContentPlacementMode::Contain: {
            const double scale = std::min(state.itemBounds.width() / fittingSize.width(),
                state.itemBounds.height() / fittingSize.height());
            placedSize = fittingSize * scale;
            break;
        }
        case ImageViewportInternal::ContentPlacementMode::Cover: {
            const double scale = std::max(state.itemBounds.width() / fittingSize.width(),
                state.itemBounds.height() / fittingSize.height());
            placedSize = fittingSize * scale;
            break;
        }
        case ImageViewportInternal::ContentPlacementMode::Stretch:
            placedSize = state.itemBounds.size();
            break;
        case ImageViewportInternal::ContentPlacementMode::Center:
            placedSize = fittingSize;
            break;
        }
    }

    double x = 0.0;
    if (state.horizontalAlignment
        == ImageViewportInternal::ContentHorizontalPlacement::AlignHCenter) {
        x = (state.itemBounds.width() - placedSize.width()) / 2.0;
    } else if (state.horizontalAlignment
        == ImageViewportInternal::ContentHorizontalPlacement::AlignRight) {
        x = state.itemBounds.width() - placedSize.width();
    }

    double y = 0.0;
    if (state.verticalAlignment == ImageViewportInternal::ContentVerticalPlacement::AlignVCenter) {
        y = (state.itemBounds.height() - placedSize.height()) / 2.0;
    } else if (state.verticalAlignment
        == ImageViewportInternal::ContentVerticalPlacement::AlignBottom) {
        y = state.itemBounds.height() - placedSize.height();
    }

    QRectF rect(x, y, placedSize.width(), placedSize.height());
    if (state.fitMode == ImageViewport::FitMode::Contain) {
        const QPointF center = rect.center();
        rect.setSize(rect.size() * state.zoom);
        rect.moveCenter(center + state.pan);
    } else {
        rect.translate(state.pan);
    }
    return rect;
}

QRectF visibleItemRectForState(const PresentationGeometry::State& state)
{
    const QRectF content = contentRectForReadyState(state);
    if (content.isEmpty()) {
        return {};
    }
    return content.intersected(state.itemBounds);
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

CoordinateResult PresentationGeometry::itemToImage(const State& state, double x, double y)
{
    return itemToPage(state, ImageViewport::PageRole::Primary, x, y);
}

CoordinateResult PresentationGeometry::imageToItem(const State& state, double x, double y)
{
    return pageToItem(state, ImageViewport::PageRole::Primary, x, y);
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
    if (!state.itemBounds.contains(itemPoint)) {
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

bool PresentationGeometry::containsVisibleImagePoint(const State& state, double x, double y)
{
    return containsVisiblePagePoint(state, ImageViewport::PageRole::Primary, x, y);
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

bool PresentationGeometry::containsVisiblePagePoint(
    const State& state, ImageViewport::PageRole role, double x, double y)
{
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    const QRectF pageRect = pageRectForRole(state, role);
    const QRectF pageLocalRect(QPointF(0.0, 0.0), pageRect.size());
    if (!containsHalfOpen(pageLocalRect, QPointF(x, y))) {
        return false;
    }

    return containsHalfOpen(visiblePageRectForState(state, role), QPointF(x, y));
}

CoordinateResult PresentationGeometry::invalidCoordinateResult() { return {}; }
