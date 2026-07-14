#include "viewportengineproviderprojection_p.h"

#include "imagesequencesource_p.h"
#include "imageviewportlimits_p.h"

ImageSequenceProviderDisplayDemand projectViewportProviderDemand(
    ViewportEngineProviderDemandInput input, ViewportEngineProviderDemandProjectionAccess access)
{
    using namespace ImageViewportInternal;
    const auto index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
    const auto& request = access.request().roles[index].activeRequest;
    const auto& source = access.request().roles[index].source;
    const auto& provider = access.providerFacts()[index];
    const auto& payload = access.display().roles[index].displayedPayload;
    QSizeF logicalSize = provider.logicalSize;
    if (logicalSize.isEmpty())
        logicalSize = sourceLogicalSize(source);
    const auto projected = projectViewportGeometryState(input.geometry, access.presentation());
    QRectF visibleRect = PresentationGeometry::visiblePageRect(projected, input.role);
    const QRectF logicalBounds(QPointF(), logicalSize);
    if (!logicalBounds.isEmpty())
        visibleRect = visibleRect.intersected(logicalBounds);
    QSizeF targetPixels = PresentationGeometry::pageItemRect(projected, input.role).size();
    if (targetPixels.isValid() && projected.devicePixelRatio > 0.0)
        targetPixels *= projected.devicePixelRatio;
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
    demand.setAllocationGeneration(input.allocationGeneration);
    demand.setCurrentPayloadQuality(payload.quality);
    demand.setCurrentPayloadExactness(payload.exactness);
    demand.setCurrentPayloadRasterSize(payload.payloadRasterSize);
    demand.setCurrentSourceToPayloadScale(payload.sourceToPayloadScale);
    return demand;
}
