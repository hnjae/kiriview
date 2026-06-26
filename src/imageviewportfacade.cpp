#include "imageviewport_p.h"

#include <QtQuick/QSGNode>

ImageViewport::ImageViewport(QQuickItem* parent)
    : QQuickItem(parent)
    , d(std::make_unique<ImageViewportPrivate>(this))
{
    setFlag(ItemHasContents, true);
}

ImageViewport::~ImageViewport() = default;

ImageSequence* ImageViewport::sequence() const { return d->sequence(); }
void ImageViewport::setSequence(ImageSequence* sequence) { d->setSequence(sequence); }
ImageViewport::RequestStatus ImageViewport::requestStatus() const { return d->requestStatus(); }
ImageViewport::RequestReason ImageViewport::requestReason() const { return d->requestReason(); }
ImageViewport::CommandReason ImageViewport::commandReason() const { return d->commandReason(); }
ImageViewport::DisplayStatus ImageViewport::displayStatus() const { return d->displayStatus(); }
ImageViewport::PlaybackPhase ImageViewport::playbackPhase() const { return d->playbackPhase(); }
int ImageViewport::displayedFrame() const { return d->displayedFrame(); }
int ImageViewport::requestedFrame() const { return d->requestedFrame(); }
int ImageViewport::displayedPosition() const { return d->displayedPosition(); }
int ImageViewport::requestedPosition() const { return d->requestedPosition(); }
int ImageViewport::frameCount() const { return d->frameCount(); }
int ImageViewport::totalDuration() const { return d->totalDuration(); }
QVariantMap ImageViewport::frameSeekBounds() const { return d->frameSeekBounds(); }
QVariantMap ImageViewport::positionSeekBounds() const { return d->positionSeekBounds(); }
ImageViewport::TriState ImageViewport::timedPlaybackSupport() const
{
    return d->timedPlaybackSupport();
}
ImageViewport::TriState ImageViewport::frameSeekSupport() const { return d->frameSeekSupport(); }
ImageViewport::TriState ImageViewport::positionSeekSupport() const
{
    return d->positionSeekSupport();
}
QSizeF ImageViewport::displayedImageSize() const { return d->displayedImageSize(); }
QRectF ImageViewport::contentRect() const { return d->contentRect(); }
QRectF ImageViewport::visibleImageRect() const { return d->visibleImageRect(); }
uint ImageViewport::displayRevision() const { return d->displayRevision(); }
uint ImageViewport::requestRevision() const { return d->requestRevision(); }
uint ImageViewport::commandRevision() const { return d->commandRevision(); }
QString ImageViewport::errorString() const { return d->errorString(); }
QString ImageViewport::warningString() const { return d->warningString(); }
ImageViewport::FillMode ImageViewport::fillMode() const { return d->fillMode(); }
void ImageViewport::setFillMode(FillMode mode) { d->setFillMode(mode); }
ImageViewport::HorizontalAlignment ImageViewport::horizontalAlignment() const
{
    return d->horizontalAlignment();
}
void ImageViewport::setHorizontalAlignment(HorizontalAlignment alignment)
{
    d->setHorizontalAlignment(alignment);
}
ImageViewport::VerticalAlignment ImageViewport::verticalAlignment() const
{
    return d->verticalAlignment();
}
void ImageViewport::setVerticalAlignment(VerticalAlignment alignment)
{
    d->setVerticalAlignment(alignment);
}
bool ImageViewport::smoothing() const { return d->smoothing(); }
void ImageViewport::setSmoothing(bool smoothing) { d->setSmoothing(smoothing); }
bool ImageViewport::mipmap() const { return d->mipmap(); }
void ImageViewport::setMipmap(bool mipmap) { d->setMipmap(mipmap); }
bool ImageViewport::mirrorHorizontally() const { return d->mirrorHorizontally(); }
void ImageViewport::setMirrorHorizontally(bool mirror) { d->setMirrorHorizontally(mirror); }
bool ImageViewport::mirrorVertically() const { return d->mirrorVertically(); }
void ImageViewport::setMirrorVertically(bool mirror) { d->setMirrorVertically(mirror); }
ImageViewport::BackgroundMode ImageViewport::backgroundMode() const { return d->backgroundMode(); }
void ImageViewport::setBackgroundMode(BackgroundMode mode) { d->setBackgroundMode(mode); }
QColor ImageViewport::backgroundColor() const { return d->backgroundColor(); }
void ImageViewport::setBackgroundColor(const QColor& color) { d->setBackgroundColor(color); }
double ImageViewport::zoom() const { return d->zoom(); }
void ImageViewport::setZoom(double zoom) { d->setZoom(zoom); }
QPointF ImageViewport::pan() const { return d->pan(); }
void ImageViewport::setPan(QPointF pan) { d->setPan(pan); }
bool ImageViewport::looping() const { return d->looping(); }
void ImageViewport::setLooping(bool looping) { d->setLooping(looping); }
ImageViewport::CommandOutcome ImageViewport::clear() { return d->clear(); }
ImageViewport::CommandOutcome ImageViewport::play() { return d->play(); }
ImageViewport::CommandOutcome ImageViewport::pause() { return d->pause(); }
ImageViewport::CommandOutcome ImageViewport::stop() { return d->stop(); }
ImageViewport::CommandOutcome ImageViewport::seek(int frame) { return d->seek(frame); }
ImageViewport::CommandOutcome ImageViewport::seekToPosition(int milliseconds)
{
    return d->seekToPosition(milliseconds);
}
ImageViewport::CommandOutcome ImageViewport::resetView() { return d->resetView(); }
QVariantMap ImageViewport::itemToImage(double x, double y) const { return d->itemToImage(x, y); }
QVariantMap ImageViewport::imageToItem(double x, double y) const { return d->imageToItem(x, y); }
bool ImageViewport::containsVisibleImagePoint(double x, double y) const
{
    return d->containsVisibleImagePoint(x, y);
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewport::advancePlaybackForTest(int elapsedMilliseconds)
{
    d->advancePlaybackForTest(elapsedMilliseconds);
}

void ImageViewport::setNextProviderRequestTokenForTest(quint64 token)
{
    d->setNextProviderRequestTokenForTest(token);
}

bool ImageViewport::hasPendingRenderCommitForTest() const
{
    return d->hasPendingRenderCommitForTest();
}
#endif

QSGNode* ImageViewport::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    return d->updatePaintNode(oldNode);
}

void ImageViewport::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    const QRectF oldItemBounds = oldGeometry.width() > 0.0 && oldGeometry.height() > 0.0
        ? QRectF(0.0, 0.0, oldGeometry.width(), oldGeometry.height())
        : QRectF();
    const QRectF oldContentRect = d->contentRectForItemBounds(oldItemBounds);
    const QRectF oldVisibleImageRect = d->visibleImageRectForItemBounds(oldItemBounds);
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    d->geometryChanged(newGeometry, oldGeometry, oldContentRect, oldVisibleImageRect);
}
