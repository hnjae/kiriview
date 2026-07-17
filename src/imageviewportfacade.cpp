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
ImageViewportCommandResult ImageViewport::clear() { return d->clear(); }
ImageViewportCommandResult ImageViewport::play(ImageViewportPageRole role) { return d->play(role); }
ImageViewportCommandResult ImageViewport::pause(ImageViewportPageRole role)
{
    return d->pause(role);
}
ImageViewportCommandResult ImageViewport::stop(ImageViewportPageRole role) { return d->stop(role); }
ImageViewportCommandResult ImageViewport::seek(ImageViewportPageRole role, int frame)
{
    return d->seek(role, frame);
}
ImageViewportCommandResult ImageViewport::seekToPosition(
    ImageViewportPageRole role, int milliseconds)
{
    return d->seekToPosition(role, milliseconds);
}
ImageViewportCommandResult ImageViewport::setPresentationTarget(
    ImageViewportPresentationTarget
        presentationTarget, // NOLINT(performance-unnecessary-value-param)
    PresentationTargetTransitionPolicy policy)
{
    return d->setPresentationTarget(presentationTarget, policy);
}
ImageViewportCommandResult ImageViewport::resetView() { return d->resetView(); }

ImageViewportCommandResult ImageViewport::setPresentation(ImageViewportPresentationCommand command)
{
    return d->setPresentation(command);
}

ImageViewportCoordinateResult ImageViewport::mapPoint(
    ImageViewportCoordinateInput input) const // NOLINT(performance-unnecessary-value-param)
{
    return d->mapPoint(input);
}

bool ImageViewport::containsPoint(
    ImageViewportCoordinateInput input) const // NOLINT(performance-unnecessary-value-param)
{
    return d->containsPoint(input);
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

void ImageViewport::itemChange(ItemChange change, const ItemChangeData& data)
{
    QQuickItem::itemChange(change, data);
    if (change == ItemDevicePixelRatioHasChanged) {
        d->devicePixelRatioChanged();
    } else if (change == ItemSceneChange) {
        d->renderAvailabilityChanged();
    }
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
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
    ImageViewport& item, ImageViewportPageRole role, quint64 token)
{
    ImageViewportPrivate::get(item)->setNextProviderRequestTokenForTest(role, token);
}

void setNextRevisionTokenForTest(ImageViewport& item, quint64 token)
{
    ImageViewportPrivate::get(item)->setNextRevisionTokenForTest(token);
}

void failNextProviderCommandDeliveryForTest(ImageViewport& item, ImageViewportPageRole role)
{
    ImageViewportPrivate::get(item)->failNextProviderCommandDeliveryForTest(role);
}

void failNextProviderQueueFlushSchedulingForTest(ImageViewport& item, ImageViewportPageRole role)
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

quint64 currentRenderAttemptForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->currentRenderAttemptForTest();
}

void reportRenderQualityFallbackForTest(
    ImageViewport& item, quint64 renderAttempt, bool smoothingUnavailable, bool mipmapUnavailable)
{
    ImageViewportPrivate::get(item)->reportRenderQualityFallbackForTest(
        renderAttempt, smoothingUnavailable, mipmapUnavailable);
}

void discardRetainedDisplayForResourcePressureForTest(ImageViewport& item)
{
    ImageViewportPrivate::get(item)->discardRetainedDisplayForResourcePressureForTest();
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

void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewportPageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    ImageViewportPrivate::get(item)->acknowledgeRenderFailureForTest(
        failedRole, generation, requestId, preparedPayloadId);
}

void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewportPageRole failedRole,
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

QVector<InternalObservationForTest> internalObservationsForTest(const ImageViewport& item)
{
    return ImageViewportPrivate::get(item)->internalObservationsForTest();
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
#endif
