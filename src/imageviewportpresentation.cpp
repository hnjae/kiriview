#include "imageviewport_p.h"
#include "presentationgeometry_p.h"

#include <algorithm>

using namespace ImageViewportInternal;

namespace {

PresentationGeometry::State geometryState(const ImageViewportPrivate& viewport)
{
    return {
        viewport.hasReadyDisplay(),
        viewport.itemBounds(),
        viewport.currentImageSize(),
        viewport.m_fillMode,
        viewport.m_horizontalAlignment,
        viewport.m_verticalAlignment,
        viewport.m_mirrorHorizontally,
        viewport.m_mirrorVertically,
        viewport.m_zoom,
        viewport.m_pan,
    };
}

PresentationGeometry::State geometryStateForItemBounds(
    const ImageViewportPrivate& viewport, const QRectF& bounds)
{
    PresentationGeometry::State state = geometryState(viewport);
    state.itemBounds = bounds;
    return state;
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

ImageViewportPrivate::FillMode ImageViewportPrivate::fillMode() const { return m_fillMode; }

void ImageViewportPrivate::setFillMode(FillMode mode)
{
    if (!isValidFillMode(mode) || m_fillMode == mode) {
        return;
    }

    m_fillMode = mode;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::HorizontalAlignment ImageViewportPrivate::horizontalAlignment() const
{
    return m_horizontalAlignment;
}

void ImageViewportPrivate::setHorizontalAlignment(HorizontalAlignment alignment)
{
    if (!isValidHorizontalAlignment(alignment) || m_horizontalAlignment == alignment) {
        return;
    }

    m_horizontalAlignment = alignment;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::VerticalAlignment ImageViewportPrivate::verticalAlignment() const
{
    return m_verticalAlignment;
}

void ImageViewportPrivate::setVerticalAlignment(VerticalAlignment alignment)
{
    if (!isValidVerticalAlignment(alignment) || m_verticalAlignment == alignment) {
        return;
    }

    m_verticalAlignment = alignment;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::smoothing() const { return m_smoothing; }

void ImageViewportPrivate::setSmoothing(bool smoothing)
{
    if (m_smoothing == smoothing) {
        return;
    }

    m_smoothing = smoothing;
    notifyPresentationChanged(false);
}

bool ImageViewportPrivate::mipmap() const { return m_mipmap; }

void ImageViewportPrivate::setMipmap(bool mipmap)
{
    if (m_mipmap == mipmap) {
        return;
    }

    m_mipmap = mipmap;
    notifyPresentationChanged(false);
}

bool ImageViewportPrivate::mirrorHorizontally() const { return m_mirrorHorizontally; }

void ImageViewportPrivate::setMirrorHorizontally(bool mirror)
{
    if (m_mirrorHorizontally == mirror) {
        return;
    }

    m_mirrorHorizontally = mirror;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::mirrorVertically() const { return m_mirrorVertically; }

void ImageViewportPrivate::setMirrorVertically(bool mirror)
{
    if (m_mirrorVertically == mirror) {
        return;
    }

    m_mirrorVertically = mirror;
    notifyPresentationChanged(true);
}

ImageViewportPrivate::BackgroundMode ImageViewportPrivate::backgroundMode() const
{
    return m_backgroundMode;
}

void ImageViewportPrivate::setBackgroundMode(BackgroundMode mode)
{
    if (!isValidBackgroundMode(mode) || m_backgroundMode == mode) {
        return;
    }

    m_backgroundMode = mode;
    notifyPresentationChanged(false);
}

QColor ImageViewportPrivate::backgroundColor() const { return m_backgroundColor; }

void ImageViewportPrivate::setBackgroundColor(const QColor& color)
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    notifyPresentationChanged(false);
}

double ImageViewportPrivate::zoom() const { return m_zoom; }

void ImageViewportPrivate::setZoom(double zoom)
{
    if (!isFinitePositive(zoom) || m_zoom == zoom) {
        return;
    }

    m_zoom = zoom;
    notifyPresentationChanged(true);
}

QPointF ImageViewportPrivate::pan() const { return m_pan; }

void ImageViewportPrivate::setPan(QPointF pan)
{
    const bool unchanged = m_pan.x() == pan.x() && m_pan.y() == pan.y();
    if (!isFinitePoint(pan) || unchanged) {
        return;
    }

    m_pan = pan;
    notifyPresentationChanged(true);
}

bool ImageViewportPrivate::looping() const { return m_looping; }

void ImageViewportPrivate::setLooping(bool looping)
{
    if (m_looping == looping) {
        return;
    }

    m_looping = looping;
    emit q->loopingChanged();
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

    return m_displayedImageSize;
}
