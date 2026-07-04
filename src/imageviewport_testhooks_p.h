#pragma once

#include "imageviewport.h"

#include <memory>

namespace ImageViewportTestHooks {

void advancePlaybackForTest(ImageViewport& item, int elapsedMilliseconds);
void setNextProviderRequestTokenForTest(ImageViewport& item, quint64 token);
void setNextProviderRequestTokenForTest(
    ImageViewport& item, ImageViewport::PageRole role, quint64 token);
void failNextProviderCommandDeliveryForTest(ImageViewport& item, ImageViewport::PageRole role);
void useSynchronousProviderExecutorForTest(ImageViewport& item);
void useSynchronousProviderQueueFlushSchedulerForTest(ImageViewport& item);
bool hasPendingRenderCommitForTest(const ImageViewport& item);
quint64 activeRequestIdForTest(const ImageViewport& item);
quint64 displayedRequestIdForTest(const ImageViewport& item);
quint64 pendingRenderGenerationForTest(const ImageViewport& item);
quint64 pendingRenderPayloadIdForTest(const ImageViewport& item);
quint64 secondaryPendingRenderPayloadIdForTest(const ImageViewport& item);
void acknowledgeRenderCommitForTest(
    ImageViewport& item, quint64 generation, quint64 requestId, quint64 preparedPayloadId);
void acknowledgeRenderCommitForTest(ImageViewport& item, quint64 generation, quint64 requestId,
    quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId);
void acknowledgeRenderFailureForTest(
    ImageViewport& item, quint64 generation, quint64 requestId, quint64 preparedPayloadId);
void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewport::PageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId);

std::unique_ptr<ImageFrame> makeImageFrameWithPayloadByteSizeForTest(
    const QImage& image, qsizetype payloadByteSize);
QImage imageForTest(const ImageFrame& frame);
QImage frameImageForTest(const ImageSequence& sequence, int frame);

} // namespace ImageViewportTestHooks
