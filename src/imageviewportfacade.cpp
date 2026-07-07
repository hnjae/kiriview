#include "imagesequence_p.h"
#include "imageviewport_p.h"
#include "imageviewport_testhooks_p.h"

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
ImageSequence* ImageViewport::primarySequence() const { return d->primarySequence(); }
ImageSequence* ImageViewport::secondarySequence() const { return d->secondarySequence(); }
ImageViewport::SpreadDirection ImageViewport::spreadDirection() const
{
    return d->spreadDirection();
}
void ImageViewport::setSpreadDirectionProperty(SpreadDirection direction)
{
    d->setSpreadDirectionProperty(direction);
}
double ImageViewport::pageGap() const { return d->pageGap(); }
void ImageViewport::setPageGapProperty(double gap) { d->setPageGapProperty(gap); }
ImageViewport::RequestStatus ImageViewport::requestStatus() const { return d->requestStatus(); }
ImageViewport::RequestReason ImageViewport::requestReason() const { return d->requestReason(); }
ImageViewport::CommandReason ImageViewport::commandReason() const { return d->commandReason(); }
ImageViewport::DisplayStatus ImageViewport::displayStatus() const { return d->displayStatus(); }
ImageViewport::PlaybackPhase ImageViewport::playbackPhase() const { return d->playbackPhase(); }
int ImageViewport::displayedFrame() const { return d->displayedFrame(); }
int ImageViewport::requestedFrame() const { return d->requestedFrame(); }
int ImageViewport::primaryDisplayedFrame() const { return d->primaryDisplayedFrame(); }
int ImageViewport::primaryRequestedFrame() const { return d->primaryRequestedFrame(); }
int ImageViewport::secondaryDisplayedFrame() const { return d->secondaryDisplayedFrame(); }
int ImageViewport::secondaryRequestedFrame() const { return d->secondaryRequestedFrame(); }
int ImageViewport::displayedPosition() const { return d->displayedPosition(); }
int ImageViewport::requestedPosition() const { return d->requestedPosition(); }
int ImageViewport::primaryDisplayedPosition() const { return d->primaryDisplayedPosition(); }
int ImageViewport::primaryRequestedPosition() const { return d->primaryRequestedPosition(); }
int ImageViewport::secondaryDisplayedPosition() const { return d->secondaryDisplayedPosition(); }
int ImageViewport::secondaryRequestedPosition() const { return d->secondaryRequestedPosition(); }
int ImageViewport::frameCount() const { return d->frameCount(); }
int ImageViewport::totalDuration() const { return d->totalDuration(); }
ImageViewportRange ImageViewport::frameSeekBounds() const { return d->frameSeekBounds(); }
ImageViewportRange ImageViewport::positionSeekBounds() const { return d->positionSeekBounds(); }
int ImageViewport::primaryFrameCount() const { return d->primaryFrameCount(); }
int ImageViewport::secondaryFrameCount() const { return d->secondaryFrameCount(); }
int ImageViewport::primaryTotalDuration() const { return d->primaryTotalDuration(); }
int ImageViewport::secondaryTotalDuration() const { return d->secondaryTotalDuration(); }
ImageViewportRange ImageViewport::primaryFrameSeekBounds() const
{
    return d->primaryFrameSeekBounds();
}
ImageViewportRange ImageViewport::secondaryFrameSeekBounds() const
{
    return d->secondaryFrameSeekBounds();
}
ImageViewportRange ImageViewport::primaryPositionSeekBounds() const
{
    return d->primaryPositionSeekBounds();
}
ImageViewportRange ImageViewport::secondaryPositionSeekBounds() const
{
    return d->secondaryPositionSeekBounds();
}
ImageViewport::TriState ImageViewport::timedPlaybackSupport() const
{
    return d->timedPlaybackSupport();
}
ImageViewport::TriState ImageViewport::frameSeekSupport() const { return d->frameSeekSupport(); }
ImageViewport::TriState ImageViewport::positionSeekSupport() const
{
    return d->positionSeekSupport();
}
ImageViewport::TriState ImageViewport::primaryTimedPlaybackSupport() const
{
    return d->primaryTimedPlaybackSupport();
}
ImageViewport::TriState ImageViewport::secondaryTimedPlaybackSupport() const
{
    return d->secondaryTimedPlaybackSupport();
}
ImageViewport::TriState ImageViewport::primaryFrameSeekSupport() const
{
    return d->primaryFrameSeekSupport();
}
ImageViewport::TriState ImageViewport::secondaryFrameSeekSupport() const
{
    return d->secondaryFrameSeekSupport();
}
ImageViewport::TriState ImageViewport::primaryPositionSeekSupport() const
{
    return d->primaryPositionSeekSupport();
}
ImageViewport::TriState ImageViewport::secondaryPositionSeekSupport() const
{
    return d->secondaryPositionSeekSupport();
}
QSizeF ImageViewport::displayedImageSize() const { return d->displayedImageSize(); }
QSizeF ImageViewport::displayedSpreadSize() const { return d->displayedSpreadSize(); }
QSizeF ImageViewport::primaryDisplayedImageSize() const { return d->primaryDisplayedImageSize(); }
QSizeF ImageViewport::secondaryDisplayedImageSize() const
{
    return d->secondaryDisplayedImageSize();
}
QRectF ImageViewport::contentRect() const { return d->contentRect(); }
QRectF ImageViewport::visibleImageRect() const { return d->visibleImageRect(); }
QRectF ImageViewport::visibleSpreadRect() const { return d->visibleSpreadRect(); }
QRectF ImageViewport::primaryPageRect() const { return d->primaryPageRect(); }
QRectF ImageViewport::secondaryPageRect() const { return d->secondaryPageRect(); }
QRectF ImageViewport::primaryItemRect() const { return d->primaryItemRect(); }
QRectF ImageViewport::secondaryItemRect() const { return d->secondaryItemRect(); }
QRectF ImageViewport::visiblePrimaryPageRect() const { return d->visiblePrimaryPageRect(); }
QRectF ImageViewport::visibleSecondaryPageRect() const { return d->visibleSecondaryPageRect(); }
PageGeometry ImageViewport::primaryPageGeometry() const { return d->primaryPageGeometry(); }
PageGeometry ImageViewport::secondaryPageGeometry() const { return d->secondaryPageGeometry(); }
QSizeF ImageViewport::contentSize() const { return d->contentSize(); }
QPointF ImageViewport::contentPosition() const { return d->contentPosition(); }
QPointF ImageViewport::maximumContentPosition() const { return d->maximumContentPosition(); }
bool ImageViewport::horizontalPannable() const { return d->horizontalPannable(); }
bool ImageViewport::verticalPannable() const { return d->verticalPannable(); }
RevisionToken ImageViewport::displayRevision() const { return d->displayRevision(); }
RevisionToken ImageViewport::requestRevision() const { return d->requestRevision(); }
RevisionToken ImageViewport::commandRevision() const { return d->commandRevision(); }
QString ImageViewport::errorString() const { return d->errorString(); }
QString ImageViewport::warningString() const { return d->warningString(); }
ImageViewport::FitMode ImageViewport::fitMode() const { return d->fitMode(); }
void ImageViewport::setFitModeProperty(FitMode mode) { d->setFitModeProperty(mode); }
double ImageViewport::zoomPercent() const { return d->zoomPercent(); }
void ImageViewport::setZoomPercentProperty(double percent) { d->setZoomPercentProperty(percent); }
int ImageViewport::rotationDegrees() const { return d->rotationDegrees(); }
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
bool ImageViewport::looping() const { return d->looping(); }
void ImageViewport::setLooping(bool looping) { d->setLooping(looping); }
ImageViewport::CommandOutcome ImageViewport::clear() { return d->clear(); }
ImageViewport::CommandOutcome ImageViewport::play() { return d->play(); }
ImageViewport::CommandOutcome ImageViewport::play(PageRole role) { return d->play(role); }
ImageViewport::CommandOutcome ImageViewport::pause() { return d->pause(); }
ImageViewport::CommandOutcome ImageViewport::pause(PageRole role) { return d->pause(role); }
ImageViewport::CommandOutcome ImageViewport::stop() { return d->stop(); }
ImageViewport::CommandOutcome ImageViewport::stop(PageRole role) { return d->stop(role); }
ImageViewport::CommandOutcome ImageViewport::seek(int frame) { return d->seek(frame); }
ImageViewport::CommandOutcome ImageViewport::seek(PageRole role, int frame)
{
    return d->seek(role, frame);
}
ImageViewport::CommandOutcome ImageViewport::seekToPosition(int milliseconds)
{
    return d->seekToPosition(milliseconds);
}
ImageViewport::CommandOutcome ImageViewport::seekToPosition(PageRole role, int milliseconds)
{
    return d->seekToPosition(role, milliseconds);
}
ImageViewport::CommandOutcome ImageViewport::setPageSet(
    const QVariant& primary, const QVariant& secondary)
{
    return d->setPageSet(primary, secondary);
}
ImageViewport::CommandOutcome ImageViewport::setPageSet(
    const QVariant& primary, const QVariant& secondary, PageSetTransitionPolicy policy)
{
    return d->setPageSet(primary, secondary, policy);
}
ImageViewport::CommandOutcome ImageViewport::setPageSet(
    ImageSequence* primary, ImageSequence* secondary)
{
    return d->setPageSet(primary, secondary);
}
ImageViewport::CommandOutcome ImageViewport::setPageSet(
    ImageSequence* primary, ImageSequence* secondary, PageSetTransitionPolicy policy)
{
    return d->setPageSet(primary, secondary, policy);
}
ImageViewport::CommandOutcome ImageViewport::setSpreadDirection(SpreadDirection direction)
{
    return d->setSpreadDirection(direction);
}
ImageViewport::CommandOutcome ImageViewport::setPageGap(double gap) { return d->setPageGap(gap); }
ImageViewport::CommandOutcome ImageViewport::setFitMode(FitMode mode, QPointF anchor)
{
    return d->setFitMode(mode, anchor);
}
ImageViewport::CommandOutcome ImageViewport::setZoomPercent(double percent, QPointF anchor)
{
    return d->setZoomPercent(percent, anchor);
}
ImageViewport::CommandOutcome ImageViewport::panBy(QPointF delta) { return d->panBy(delta); }
ImageViewport::CommandOutcome ImageViewport::panToStart() { return d->panToStart(); }
ImageViewport::CommandOutcome ImageViewport::panToEnd() { return d->panToEnd(); }
ImageViewport::CommandOutcome ImageViewport::scanNext() { return d->scanNext(); }
ImageViewport::CommandOutcome ImageViewport::scanPrevious() { return d->scanPrevious(); }
ImageViewport::CommandOutcome ImageViewport::rotateClockwise(QPointF anchor)
{
    return d->rotateClockwise(anchor);
}
ImageViewport::CommandOutcome ImageViewport::rotateCounterClockwise(QPointF anchor)
{
    return d->rotateCounterClockwise(anchor);
}
ImageViewport::CommandOutcome ImageViewport::setMirrorHorizontally(bool enabled, QPointF anchor)
{
    return d->setMirrorHorizontally(enabled, anchor);
}
ImageViewport::CommandOutcome ImageViewport::setMirrorVertically(bool enabled, QPointF anchor)
{
    return d->setMirrorVertically(enabled, anchor);
}
ImageViewport::CommandOutcome ImageViewport::resetView() { return d->resetView(); }
CoordinateResult ImageViewport::itemToSpread(double x, double y) const
{
    return d->itemToSpread(x, y);
}
CoordinateResult ImageViewport::spreadToItem(double x, double y) const
{
    return d->spreadToItem(x, y);
}
CoordinateResult ImageViewport::itemToPage(PageRole role, double x, double y) const
{
    return d->itemToPage(role, x, y);
}
CoordinateResult ImageViewport::pageToItem(PageRole role, double x, double y) const
{
    return d->pageToItem(role, x, y);
}
PageGeometry ImageViewport::pageGeometry(PageRole role) const
{
    return d->pageGeometry(role);
}
bool ImageViewport::containsVisibleSpreadPoint(double x, double y) const
{
    return d->containsVisibleSpreadPoint(x, y);
}
bool ImageViewport::containsVisiblePagePoint(PageRole role, double x, double y) const
{
    return d->containsVisiblePagePoint(role, x, y);
}
CoordinateResult ImageViewport::itemToImage(double x, double y) const
{
    return d->itemToImage(x, y);
}
CoordinateResult ImageViewport::imageToItem(double x, double y) const
{
    return d->imageToItem(x, y);
}
bool ImageViewport::containsVisibleImagePoint(double x, double y) const
{
    return d->containsVisibleImagePoint(x, y);
}

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

namespace ImageViewportTestHooks {

void advancePlaybackForTest(ImageViewport& item, int elapsedMilliseconds)
{
    ImageViewportPrivate::get(item)->advancePlaybackForTest(elapsedMilliseconds);
}

void setNextProviderRequestTokenForTest(ImageViewport& item, quint64 token)
{
    ImageViewportPrivate::get(item)->setNextProviderRequestTokenForTest(token);
}

void setNextProviderRequestTokenForTest(
    ImageViewport& item, ImageViewport::PageRole role, quint64 token)
{
    ImageViewportPrivate::get(item)->setNextProviderRequestTokenForTest(role, token);
}

void setNextRevisionTokenForTest(ImageViewport& item, quint64 token)
{
    ImageViewportPrivate::get(item)->setNextRevisionTokenForTest(token);
}

void failNextProviderCommandDeliveryForTest(ImageViewport& item, ImageViewport::PageRole role)
{
    ImageViewportPrivate::get(item)->failNextProviderCommandDeliveryForTest(role);
}

void failNextProviderQueueFlushSchedulingForTest(
    ImageViewport& item, ImageViewport::PageRole role)
{
    ImageViewportPrivate::get(item)->failNextProviderQueueFlushSchedulingForTest(role);
}

void useSynchronousProviderExecutorForTest(ImageViewport& item)
{
    ImageViewportPrivate::get(item)->useSynchronousProviderExecutorForTest();
}

void useSynchronousProviderEventDeliveryForTest(ImageViewport& item)
{
    ImageViewportPrivate::get(item)->useSynchronousProviderEventDeliveryForTest();
}

void useSynchronousProviderQueueFlushSchedulerForTest(ImageViewport& item)
{
    ImageViewportPrivate::get(item)->useSynchronousProviderQueueFlushSchedulerForTest();
}

bool hasPendingRenderCommitForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->hasPendingRenderCommitForTest();
}

quint64 activeRequestIdForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->activeRequestIdForTest();
}

quint64 displayedRequestIdForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->displayedRequestIdForTest();
}

quint64 pendingRenderGenerationForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->pendingRenderGenerationForTest();
}

quint64 pendingRenderPayloadIdForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->pendingRenderPayloadIdForTest();
}

quint64 secondaryPendingRenderPayloadIdForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->secondaryPendingRenderPayloadIdForTest();
}

void acknowledgeRenderCommitForTest(
    ImageViewport& item, quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    ImageViewportPrivate::get(item)->acknowledgeRenderCommitForTest(
        generation, requestId, preparedPayloadId);
}

void acknowledgeRenderCommitForTest(ImageViewport& item, quint64 generation, quint64 requestId,
    quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId)
{
    ImageViewportPrivate::get(item)->acknowledgeRenderCommitForTest(
        generation, requestId, primaryPreparedPayloadId, secondaryPreparedPayloadId);
}

void acknowledgeRenderFailureForTest(
    ImageViewport& item, quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    ImageViewportPrivate::get(item)->acknowledgeRenderFailureForTest(
        generation, requestId, preparedPayloadId);
}

void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewport::PageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    ImageViewportPrivate::get(item)->acknowledgeRenderFailureForTest(
        failedRole, generation, requestId, preparedPayloadId);
}

void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewport::PageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId, RenderFailureCause cause)
{
    ImageViewportPrivate::get(item)->acknowledgeRenderFailureForTest(
        failedRole, generation, requestId, preparedPayloadId, cause);
}

RenderFailureDiagnosticForTest lastAcceptedRenderFailureDiagnosticForTest(
    const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->lastAcceptedRenderFailureDiagnosticForTest();
}

ProviderTransportDiagnosticForTest lastProviderTransportDiagnosticForTest(
    const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->lastProviderTransportDiagnosticForTest();
}

ProviderSchedulerDiagnosticForTest lastProviderSchedulerDiagnosticForTest(
    const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->lastProviderSchedulerDiagnosticForTest();
}

std::unique_ptr<ImageFrame> makeImageFrameWithPayloadByteSizeForTest(
    const QImage& image, qsizetype payloadByteSize)
{
    return ImageViewportInternal::ImageFramePrivateAccess::createWithPayloadByteSize(
        image, payloadByteSize);
}

QImage imageForTest(const ImageFrame& frame)
{
    return ImageViewportInternal::ImageFramePrivateAccess::image(frame);
}

} // namespace ImageViewportTestHooks
