#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"

#include "imagesequencesource_p.h"
#include "viewportgeometryhelpers_p.h"
#include "viewportenginerenderackhelpers_p.h"

namespace {
using namespace ImageViewportInternal;

bool hasSecondary(const RequestState& request)
{
    return request.roles[1].sequence && request.roles[1].activeRequest.target.frame >= 0;
}

bool hasDisplayable(const RequestState& request) { return request.roles[0].source.facts.present; }

bool waitingForRender(const RequestState& request)
{
    return request.status == ImageViewport::RequestStatus::Loading
        && (request.reason == ImageViewport::RequestReason::UploadPending
            || request.reason == ImageViewport::RequestReason::RenderWaiting);
}

bool pendingSpreadReady(const DisplayState& display, const RequestState& request)
{
    return display.roles[0].pendingRenderPayload.commitPending
        && !display.roles[0].pendingRenderPayload.image.isNull()
        && (!hasSecondary(request) || !display.roles[1].pendingRenderPayload.image.isNull());
}

void stageBuiltIn(RequestState& request, DisplayState& display)
{
    display.captureRenderFailureRetainedDisplay(hasDisplayable(request));
    display.roles[0].pendingRenderPayload.commitPending = true;
    display.beginPreparedPayloadIdentity(request.sequenceGeneration, request.roles[0].activeRequest);
    if (request.roles[0].activeRequest.target.frame >= 0) {
        display.roles[0].pendingRenderPayload = FramePreparation::admitBuiltInFrame(request.roles[0].source,
            request.roles[0].activeRequest.target.frame, display.roles[0].pendingRenderPayload)
                                           .preparedPayload;
    }
    if (hasSecondary(request) && !request.roles[1].provider) {
        auto& secondary = display.roles[1].pendingRenderPayload;
        secondary.commitPending = true;
        secondary.generation = request.sequenceGeneration;
        secondary.requestId = request.roles[0].activeRequest.identity.id;
        secondary.payloadId = ++display.nextPreparedPayloadId;
        request.roles[1].activeRequest.preparedPayloadId = secondary.payloadId;
        secondary = FramePreparation::admitBuiltInFrame(
            request.roles[1].source, request.roles[1].activeRequest.target.frame, secondary)
                        .preparedPayload;
    }
}

}

ViewportRenderSynchronization ViewportEngine::beginRenderSynchronization(
    const RenderSynchronizationInput& input)
{
    return synchronizeViewportEngineRender(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->presentationState.presentation, m_state->renderCoordination });
}

ViewportEngineRenderCommitTransition ViewportEngine::acknowledgeRenderCommit(
    const RenderAcknowledgementInput& input)
{
    auto reduction = reduceViewportEngineRenderCommit(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, m_state->providerState.roles,
            m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? playbackScheduleEffect()
                                        : ViewportPlaybackScheduleEffect {} };
}

ViewportEngineRenderFailureTransition ViewportEngine::acknowledgeRenderFailure(
    const RenderAcknowledgementInput& input)
{
    auto reduction = reduceViewportEngineRenderFailure(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? playbackScheduleEffect()
                                        : ViewportPlaybackScheduleEffect {},
        reduction.diagnostic };
}

ViewportEngine::GeometryChangeResult ViewportEngine::handleGeometryChanged(
    const GeometryChangeInput& input)
{
    GeometryChangeResult result;
    auto& changes = result.changes;
    const GeometryInput demandGeometry { input.geometryState.hasReadyDisplay,
        input.geometryState.itemBounds, input.geometryState.primaryImageSize,
        input.geometryState.secondaryImageSize, input.geometryState.devicePixelRatio };
    if (hasDisplayable(renderAccess().request()) && waitingForRender(renderAccess().request())
        && !input.itemBounds.isEmpty()) {
        if (pendingSpreadReady(renderAccess().display(), renderAccess().request())) {
            changes.scheduleUpdate = true;
            result.providerEffects = restageProviderDemands(demandGeometry);
            return result;
        }
        if (!renderAccess().request().roles[0].source.facts.provider) {
            stageBuiltIn(renderAccess().request(), renderAccess().display());
            renderAccess().request().status = ImageViewport::RequestStatus::Loading;
            renderAccess().request().reason = ImageViewport::RequestReason::UploadPending;
            renderAccess().display().status = renderAccess().display().hasReadyDisplay(hasDisplayable(renderAccess().request()))
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            changes.requestState = true;
            changes.requestRevision = true;
            changes.displayState = true;
            changes.displayRevision = true;
            changes.scheduleUpdate = true;
            result.providerEffects = restageProviderDemands(demandGeometry);
            return result;
        }
    } else if (renderAccess().request().roles[0].source.facts.provider
        && renderAccess().request().status == ImageViewport::RequestStatus::Loading
        && renderAccess().request().reason == ImageViewport::RequestReason::UploadPending
        && input.itemBounds.isEmpty() && !renderAccess().display().roles[0].pendingRenderPayload.image.isNull()) {
        renderAccess().request().reason = ImageViewport::RequestReason::RenderWaiting;
        changes.requestState = true;
        changes.requestRevision = true;
        changes.displayRevision = true;
    } else {
        changes.displayRevision = true;
    }
    changes.geometryState
        = rectsDifferExactly(
              PresentationGeometry::contentRect(input.geometryState), input.oldContentRect)
        || rectsDifferExactly(
            PresentationGeometry::visibleImageRect(input.geometryState), input.oldVisibleImageRect);
    changes.scheduleUpdate = true;
    result.providerEffects = restageProviderDemands(demandGeometry);
    return result;
}
