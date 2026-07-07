#pragma once

#include "imageviewport.h"
#include "imageviewportstate_p.h"

#include <memory>

namespace ImageViewportTestHooks {

using RenderFailureDiagnosticForTest = ImageViewportInternal::RenderFailureDiagnostic;
using ProviderTransportOperationForTest = ImageViewportInternal::ProviderTransportOperation;
using ProviderTransportDiagnosticForTest = ImageViewportInternal::ProviderTransportDiagnostic;
using ProviderSchedulerOperationForTest = ImageViewportInternal::ProviderSchedulerOperation;
using ProviderSchedulerDiagnosticForTest = ImageViewportInternal::ProviderSchedulerDiagnostic;

void advancePlaybackForTest(ImageViewport& item, int elapsedMilliseconds);
void setNextProviderRequestTokenForTest(ImageViewport& item, quint64 token);
void setNextProviderRequestTokenForTest(
    ImageViewport& item, ImageViewport::PageRole role, quint64 token);
void setNextRevisionTokenForTest(ImageViewport& item, quint64 token);
void failNextProviderCommandDeliveryForTest(ImageViewport& item, ImageViewport::PageRole role);
void failNextProviderQueueFlushSchedulingForTest(
    ImageViewport& item, ImageViewport::PageRole role);
void useSynchronousProviderExecutorForTest(ImageViewport& item);
void useSynchronousProviderEventDeliveryForTest(ImageViewport& item);
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
void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewport::PageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId, RenderFailureCause cause);
RenderFailureDiagnosticForTest lastAcceptedRenderFailureDiagnosticForTest(
    const ImageViewport& item);
ProviderTransportDiagnosticForTest lastProviderTransportDiagnosticForTest(
    const ImageViewport& item);
ProviderSchedulerDiagnosticForTest lastProviderSchedulerDiagnosticForTest(
    const ImageViewport& item);

std::unique_ptr<ImageFrame> makeImageFrameWithPayloadByteSizeForTest(
    const QImage& image, qsizetype payloadByteSize);
QImage imageForTest(const ImageFrame& frame);

} // namespace ImageViewportTestHooks
