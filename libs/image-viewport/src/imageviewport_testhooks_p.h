/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewportstate_p.h"
#include "imageviewporttoken_p.h"
#include "internalobservation_p.h"
#include <ImageViewport/imageviewport.h>

#include <memory>

namespace ImageViewportTestHooks {

using RenderFailureDiagnosticForTest = ImageViewportInternal::RenderFailureDiagnostic;
using ProviderTransportOperationForTest = ImageViewportInternal::ProviderTransportOperation;
using ProviderTransportDiagnosticForTest = ImageViewportInternal::ProviderTransportDiagnostic;
using ProviderSchedulerOperationForTest = ImageViewportInternal::ProviderSchedulerOperation;
using ProviderSchedulerDiagnosticForTest = ImageViewportInternal::ProviderSchedulerDiagnostic;
using InternalObservationForTest = ImageViewportInternal::InternalObservation;
using InternalObservationSubsystemForTest = ImageViewportInternal::InternalObservationSubsystem;
using InternalObservationCategoryForTest = ImageViewportInternal::InternalObservationCategory;
using InternalObservationCauseForTest = ImageViewportInternal::InternalObservationCause;

void advancePlaybackForTest(ImageViewport& item, int elapsedMilliseconds,
    ImageViewportPageRole role = ImageViewportPageRole::Primary);
void setPendingPlaybackSchedulerElapsedForTest(ImageViewport& item, int elapsedMilliseconds,
    ImageViewportPageRole role = ImageViewportPageRole::Primary);
void setNextProviderRequestTokenForTest(ImageViewport& item, quint64 token);
void setNextProviderRequestTokenForTest(
    ImageViewport& item, ImageViewportPageRole role, quint64 token);
void setNextRevisionTokenForTest(ImageViewport& item, quint64 token);
void failNextProviderCommandDeliveryForTest(ImageViewport& item, ImageViewportPageRole role);
void failNextProviderQueueFlushSchedulingForTest(ImageViewport& item, ImageViewportPageRole role);
void useSynchronousProviderExecutorForTest(ImageViewport& item);
void useSynchronousProviderEventDeliveryForTest(ImageViewport& item);
void useSynchronousProviderQueueFlushSchedulerForTest(ImageViewport& item);
ImageSequenceProviderRequestToken providerRequestTokenForTest(quint64 token);
quint64 providerRequestTokenValueForTest(ImageSequenceProviderRequestToken token);
RevisionToken revisionTokenForTest(quint64 token);
quint64 revisionTokenValueForTest(RevisionToken token);
quint64 revisionTokenValueForTest(ImageViewportRevisionToken token);
bool hasPendingRenderCommitForTest(const ImageViewport& item);
quint64 activeRequestIdForTest(const ImageViewport& item);
quint64 displayedRequestIdForTest(const ImageViewport& item);
quint64 pendingRenderGenerationForTest(const ImageViewport& item);
quint64 pendingRenderPayloadIdForTest(const ImageViewport& item);
quint64 secondaryPendingRenderPayloadIdForTest(const ImageViewport& item);
quint64 currentRenderAttemptForTest(const ImageViewport& item);
void reportRenderQualityFallbackForTest(
    ImageViewport& item, quint64 renderAttempt, bool smoothingUnavailable, bool mipmapUnavailable);
void discardRetainedDisplayForResourcePressureForTest(ImageViewport& item);
void acknowledgeRenderCommitForTest(
    ImageViewport& item, quint64 generation, quint64 requestId, quint64 preparedPayloadId);
void acknowledgeRenderCommitForTest(ImageViewport& item, quint64 generation, quint64 requestId,
    quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId);
void acknowledgeRenderFailureForTest(
    ImageViewport& item, quint64 generation, quint64 requestId, quint64 preparedPayloadId);
void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewportPageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId);
void acknowledgeRenderFailureForTest(ImageViewport& item, ImageViewportPageRole failedRole,
    quint64 generation, quint64 requestId, quint64 preparedPayloadId, RenderFailureCause cause);
RenderFailureDiagnosticForTest lastAcceptedRenderFailureDiagnosticForTest(
    const ImageViewport& item);
ProviderTransportDiagnosticForTest lastProviderTransportDiagnosticForTest(
    const ImageViewport& item);
ProviderSchedulerDiagnosticForTest lastProviderSchedulerDiagnosticForTest(
    const ImageViewport& item);
QVector<InternalObservationForTest> internalObservationsForTest(const ImageViewport& item);

std::unique_ptr<ImageFrame> makeImageFrameWithPayloadByteSizeForTest(
    const QImage& image, qsizetype payloadByteSize);
QImage imageForTest(const ImageFrame& frame);

} // namespace ImageViewportTestHooks
