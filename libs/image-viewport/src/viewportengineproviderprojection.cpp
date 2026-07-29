// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengineproviderprojection_p.h"

#include "imagesequencesource_p.h"
#include "imageviewportlimits_p.h"
#include "imageviewporttoken_p.h"

#include <cmath>

namespace {
bool positiveFinite(QSizeF size)
{
    return std::isfinite(size.width()) && std::isfinite(size.height()) && size.width() > 0.0
        && size.height() > 0.0;
}

bool finiteRect(const QRectF& rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y()) && std::isfinite(rect.width())
        && std::isfinite(rect.height()) && std::isfinite(rect.right())
        && std::isfinite(rect.bottom());
}
}

ImageSequenceProviderDisplayDemand projectViewportProviderDemand(
    ViewportEngineProviderDemandInput input, ViewportEngineProviderDemandProjectionAccess access)
{
    using namespace ImageViewportInternal;
    const auto index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
    const auto& request = access.request().roles[index].activeRequest;
    const auto& source = access.request().roles[index].source;
    const auto& provider = access.providerFacts()[index];
    const auto& displayedRole = access.display().roles[index];
    const bool payloadMatchesTarget
        = displayedRole.displayedRequest.generation == access.request().sequenceGeneration
        && displayedRole.displayedRequest.request.resolvedFrame.frame == request.resolvedFrame.frame
        && displayedRole.displayedRequest.request.resolvedFrame.position
            == request.resolvedFrame.position;
    const PreparedPayload payload
        = payloadMatchesTarget ? displayedRole.displayedPayload : PreparedPayload {};
    QSizeF logicalSize = provider.logicalSize;
    if (!positiveFinite(logicalSize))
        logicalSize = sourceLogicalSize(source);
    if (!positiveFinite(logicalSize))
        logicalSize = {};
    const auto projected = projectViewportGeometryState(input.geometry, access.presentation());
    QRectF visibleRect = PresentationGeometry::visiblePageRect(projected, input.role);
    const QRectF logicalBounds(QPointF(), logicalSize);
    if (!logicalBounds.isEmpty())
        visibleRect = visibleRect.intersected(logicalBounds);
    if (!finiteRect(visibleRect))
        visibleRect = {};
    QSizeF targetPixels = PresentationGeometry::pageItemRect(projected, input.role).size();
    if (positiveFinite(targetPixels) && std::isfinite(projected.devicePixelRatio)
        && projected.devicePixelRatio > 0.0) {
        targetPixels *= projected.devicePixelRatio;
    }
    if (!positiveFinite(targetPixels))
        targetPixels = {};
    ImageSequenceProviderDisplayDemand demand;
    demand.setDemandRevision(input.demandRevision);
    demand.setRequestRevision(input.requestRevision);
    demand.setPresentationRevision(input.presentationRevision);
    demand.setRole(input.role);
    demand.setResolvedFrame(request.resolvedFrame.frame);
    demand.setRequestedPosition(
        request.target.providerTargetKind == ProviderRequestTargetKind::Frame
            ? request.resolvedFrame.position
            : request.target.position);
    demand.setSourceLogicalSize(logicalSize);
    demand.setVisibleSourceRect(visibleRect);
    demand.setTargetDisplaySizePixels(targetPixels);
    demand.setEffectiveDevicePixelRatio(projected.devicePixelRatio);
    demand.setRotationDegrees(access.presentation().rotationDegrees);
    demand.setMirrorHorizontally(access.presentation().mirrorHorizontally);
    demand.setMirrorVertically(access.presentation().mirrorVertically);
    demand.setQualityPreference(access.presentation().qualityPreference);
    demand.setExactnessPreference(access.presentation().exactnessPreference);
    demand.setMaximumPayloadBytes(ImageSequenceLimits::maximumPayloadBytes());
    demand.setDisplayByteBudget(access.display().payloadAllocation.roleBudgets[index]);
    demand.setAllocationGeneration(access.display().payloadAllocation.generation != 0
            ? AllocationGenerationTokenPrivateAccess::fromValue(
                  access.display().payloadAllocation.generation)
            : input.allocationGeneration);
    demand.setCurrentPayloadQuality(payload.quality);
    demand.setCurrentPayloadExactness(payload.exactness);
    demand.setCurrentPayloadRasterSize(payload.payloadRasterSize);
    return demand;
}
