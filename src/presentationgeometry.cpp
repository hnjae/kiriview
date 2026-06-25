#include "presentationgeometry_p.h"

#include <algorithm>
#include <cmath>

namespace {

QRectF contentRectForReadyState(const PresentationGeometry::State &state)
{
    if (!state.hasReadyDisplay || state.itemBounds.isEmpty() || state.imageSize.isEmpty()) {
        return {};
    }

    QSizeF placedSize;
    switch (state.fillMode) {
    case ImageViewport::FillMode::Contain: {
        const double scale = std::min(state.itemBounds.width() / state.imageSize.width(), state.itemBounds.height() / state.imageSize.height());
        placedSize = state.imageSize * scale;
        break;
    }
    case ImageViewport::FillMode::Cover: {
        const double scale = std::max(state.itemBounds.width() / state.imageSize.width(), state.itemBounds.height() / state.imageSize.height());
        placedSize = state.imageSize * scale;
        break;
    }
    case ImageViewport::FillMode::Stretch:
        placedSize = state.itemBounds.size();
        break;
    case ImageViewport::FillMode::Center:
        placedSize = state.imageSize;
        break;
    }

    double x = 0.0;
    if (state.horizontalAlignment == ImageViewport::HorizontalAlignment::AlignHCenter) {
        x = (state.itemBounds.width() - placedSize.width()) / 2.0;
    } else if (state.horizontalAlignment == ImageViewport::HorizontalAlignment::AlignRight) {
        x = state.itemBounds.width() - placedSize.width();
    }

    double y = 0.0;
    if (state.verticalAlignment == ImageViewport::VerticalAlignment::AlignVCenter) {
        y = (state.itemBounds.height() - placedSize.height()) / 2.0;
    } else if (state.verticalAlignment == ImageViewport::VerticalAlignment::AlignBottom) {
        y = state.itemBounds.height() - placedSize.height();
    }

    QRectF rect(x, y, placedSize.width(), placedSize.height());
    const QPointF center = rect.center();
    rect.setSize(rect.size() * state.zoom);
    rect.moveCenter(center + state.pan);
    return rect;
}

}

QRectF PresentationGeometry::contentRect(const State &state)
{
    return contentRectForReadyState(state);
}

QRectF PresentationGeometry::visibleImageRect(const State &state)
{
    if (!state.hasReadyDisplay || state.itemBounds.isEmpty()) {
        return {};
    }

    const QRectF content = contentRectForReadyState(state);
    if (content.isEmpty()) {
        return {};
    }

    const QRectF visibleItemRect = content.intersected(state.itemBounds);
    if (visibleItemRect.isEmpty()) {
        return {};
    }

    const double x = (visibleItemRect.x() - content.x()) / content.width() * state.imageSize.width();
    const double y = (visibleItemRect.y() - content.y()) / content.height() * state.imageSize.height();
    const double width = visibleItemRect.width() / content.width() * state.imageSize.width();
    const double height = visibleItemRect.height() / content.height() * state.imageSize.height();
    QRectF visibleImageRect(x, y, width, height);
    if (state.mirrorHorizontally) {
        visibleImageRect.moveLeft(state.imageSize.width() - visibleImageRect.right());
    }
    if (state.mirrorVertically) {
        visibleImageRect.moveTop(state.imageSize.height() - visibleImageRect.bottom());
    }
    return visibleImageRect;
}

QVariantMap PresentationGeometry::itemToImage(const State &state, double x, double y)
{
    if (!state.hasReadyDisplay || !std::isfinite(x) || !std::isfinite(y)) {
        return invalidCoordinateResult();
    }

    const QRectF content = contentRectForReadyState(state);
    if (state.itemBounds.isEmpty() || content.isEmpty() || x < state.itemBounds.left() || y < state.itemBounds.top() || x >= state.itemBounds.right() || y >= state.itemBounds.bottom()) {
        return invalidCoordinateResult();
    }

    double imageX = (x - content.x()) / content.width() * state.imageSize.width();
    double imageY = (y - content.y()) / content.height() * state.imageSize.height();
    if (state.mirrorHorizontally) {
        imageX = state.imageSize.width() - imageX;
    }
    if (state.mirrorVertically) {
        imageY = state.imageSize.height() - imageY;
    }

    if (!containsVisibleImagePoint(state, imageX, imageY)) {
        return invalidCoordinateResult();
    }

    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("x"), imageX},
        {QStringLiteral("y"), imageY},
    };
}

QVariantMap PresentationGeometry::imageToItem(const State &state, double x, double y)
{
    if (!containsVisibleImagePoint(state, x, y)) {
        return invalidCoordinateResult();
    }

    const QRectF content = contentRectForReadyState(state);
    double normalizedX = x;
    double normalizedY = y;
    if (state.mirrorHorizontally) {
        normalizedX = state.imageSize.width() - x;
    }
    if (state.mirrorVertically) {
        normalizedY = state.imageSize.height() - y;
    }

    const double itemX = content.x() + normalizedX / state.imageSize.width() * content.width();
    const double itemY = content.y() + normalizedY / state.imageSize.height() * content.height();
    if (!state.itemBounds.contains(QPointF(itemX, itemY))) {
        return invalidCoordinateResult();
    }

    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("x"), itemX},
        {QStringLiteral("y"), itemY},
    };
}

bool PresentationGeometry::containsVisibleImagePoint(const State &state, double x, double y)
{
    if (!state.hasReadyDisplay || !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }

    if (x < 0.0 || y < 0.0 || x >= state.imageSize.width() || y >= state.imageSize.height()) {
        return false;
    }

    const QRectF visible = visibleImageRect(state);
    return x >= visible.left()
        && y >= visible.top()
        && x < visible.right()
        && y < visible.bottom();
}

QVariantMap PresentationGeometry::invalidCoordinateResult()
{
    return {
        {QStringLiteral("valid"), false},
        {QStringLiteral("x"), 0.0},
        {QStringLiteral("y"), 0.0},
    };
}
