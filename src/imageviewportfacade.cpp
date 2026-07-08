#include "imagesequence_p.h"
#include "imageviewport_p.h"
#include "imageviewport_testhooks_p.h"
#include "imageviewporttoken_p.h"

#include <QtQuick/QSGNode>

ImageViewport::ImageViewport(QQuickItem* parent)
    : QQuickItem(parent)
    , d(std::make_unique<ImageViewportPrivate>(this))
{
    setFlag(ItemHasContents, true);
}

ImageViewport::~ImageViewport() = default;

ImageViewportStateSnapshot ImageViewport::state() const { return d->state(); }
void ImageViewport::setSequence(ImageSequence* sequence) { d->setSequence(sequence); }
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
ImageViewport::CommandOutcome ImageViewport::setPageSet(const QVariant& pageSet)
{
    return d->setPageSet(pageSet);
}
ImageViewport::CommandOutcome ImageViewport::setPageSet(ImageViewportPageSet pageSet)
{
    return d->setPageSet(pageSet);
}
ImageViewport::CommandOutcome ImageViewport::setPageSet(
    ImageViewportPageSet pageSet, PageSetTransitionPolicy policy)
{
    return d->setPageSet(pageSet, policy);
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
ImageViewport::CommandOutcome ImageViewport::resetView() { return d->resetView(); }

ImageViewport::CommandOutcome ImageViewport::setPresentation(
    ImageViewportPresentationCommand command)
{
    return d->setPresentation(command);
}

ImageViewportCoordinateResult ImageViewport::mapPoint(ImageViewportCoordinateInput input) const
{
    return d->mapPoint(std::move(input));
}

bool ImageViewport::containsPoint(ImageViewportCoordinateInput input) const
{
    return d->containsPoint(std::move(input));
}

ImageViewportCoordinateResult ImageViewport::nearestVisiblePoint(
    ImageViewportCoordinateInput input) const
{
    return d->nearestVisiblePoint(std::move(input));
}

QSGNode* ImageViewport::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    return d->renderHost.updatePaintNode(oldNode);
}

void ImageViewport::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    const QRectF oldItemBounds = oldGeometry.width() > 0.0 && oldGeometry.height() > 0.0
        ? QRectF(0.0, 0.0, oldGeometry.width(), oldGeometry.height())
        : QRectF();
    const QRectF oldContentRect = d->contentRectForItemBounds(oldItemBounds);
    const QRectF oldVisibleImageRect = d->visibleImageRectForItemBounds(oldItemBounds);
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    d->renderHost.geometryChanged(newGeometry, oldGeometry, oldContentRect, oldVisibleImageRect);
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

void failNextProviderQueueFlushSchedulingForTest(ImageViewport& item, ImageViewport::PageRole role)
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

ImageSequenceProviderRequestToken providerRequestTokenForTest(quint64 token)
{
    return ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(token);
}

quint64 providerRequestTokenValueForTest(ImageSequenceProviderRequestToken token)
{
    return ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(token);
}

RevisionToken revisionTokenForTest(quint64 token)
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(token);
}

quint64 revisionTokenValueForTest(RevisionToken token)
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::value(token);
}

quint64 revisionTokenValueForTest(ImageViewportRevisionToken token)
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::value(token);
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

RenderFailureDiagnosticForTest lastAcceptedRenderFailureDiagnosticForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->lastAcceptedRenderFailureDiagnosticForTest();
}

ProviderTransportDiagnosticForTest lastProviderTransportDiagnosticForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->lastProviderTransportDiagnosticForTest();
}

ProviderSchedulerDiagnosticForTest lastProviderSchedulerDiagnosticForTest(const ImageViewport& item)
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
