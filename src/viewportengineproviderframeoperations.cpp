#include "viewportengineproviderframeoperations_p.h"

#include "presentationgeometry_p.h"
#include "viewportengineprojection_p.h"

namespace {
using namespace ImageViewportInternal;

DisplayRequest& requestForRole(RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.roles[1].activeRequest
                                                    : request.roles[0].activeRequest;
}

const DisplayRequest& requestForRole(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.roles[1].activeRequest
                                                    : request.roles[0].activeRequest;
}

bool providerPresent(const RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary
        ? request.roles[0].source.facts.provider
        : request.roles[1].sequence && request.roles[1].provider;
}

bool terminalMatchesActiveRequest(const RequestState& request)
{
    const auto& terminal = request.targetSpreadTerminal;
    return terminal.sealed && terminal.generation == request.sequenceGeneration
        && terminal.requestId == request.roles[0].activeRequest.identity.id;
}

bool activeTokenMatches(const ProviderRoleState& provider, const RequestState& request,
    ImageViewportPageRole role, ImageSequenceProviderRequestToken token)
{
    const auto& active = requestForRole(request, role);
    return provider.requests.activeFrameToken.isValid()
        && token == provider.requests.activeFrameToken && token.isValid()
        && token == active.providerFrameToken;
}

bool displayedPrimaryPayloadMatchesActiveTarget(
    const DisplayState& display, const RequestState& request)
{
    const auto& active = request.roles[0].activeRequest;
    return display.hasReadyDisplay(request.roles[0].source.facts.present)
        && display.roles[0].displayedRequest.generation == request.sequenceGeneration
        && display.roles[0].displayedRequest.request.resolvedFrame.frame
        == active.resolvedFrame.frame
        && display.roles[0].displayedRequest.request.resolvedFrame.position
        == active.resolvedFrame.position;
}

FramePreparation::ProviderFrameState preparationState(const RequestState& request,
    const DisplayState& display, const ProviderRoleState& provider,
    const PresentationState& presentation, ImageViewportPageRole role)
{
    const auto& active = requestForRole(request, role);
    PreparedPayload preparedPayload = display.roles[0].pendingRenderPayload;
    if (role == ImageViewportPageRole::Primary && !preparedPayload.identity().isValid()) {
        preparedPayload.generation = request.sequenceGeneration;
        preparedPayload.requestId = active.identity.id;
        preparedPayload.payloadId
            = preparedPayload.requestId == 0 ? 0 : display.nextPreparedPayloadId + 1;
    }
    return { provider.facts.metadataReady, provider.facts.timedMetadata, provider.facts.logicalSize,
        provider.facts.timingIntervals, active.resolvedFrame, preparedPayload,
        active.demandRevision, presentation.exactnessPreference };
}

void clearQueue(ProviderRequestState& requests)
{
    requests.queuedFrameRequest = false;
    requests.queuedFrameGeneration = 0;
    requests.queuedFrameRequestId = 0;
    requests.queuedFrame = -1;
    requests.queuedPosition = -1;
    requests.queuedResolvedFrame = {};
    requests.queuedFrameFromPlayback = false;
    requests.queuedFrameTargetKind = ProviderRequestTargetKind::Unknown;
}

ImageViewport::DisplayStatus retainedDisplayStatus(const DisplayState& display)
{
    const bool retained = (display.status == ImageViewport::DisplayStatus::Ready
                              || display.status == ImageViewport::DisplayStatus::Retained)
        && display.roles[0].displayedImageSize.isValid();
    return retained ? ImageViewport::DisplayStatus::Retained : ImageViewport::DisplayStatus::Empty;
}

void updatePlaybackPhase(
    PlaybackState& playback, ImageViewport::PlaybackPhase phase, ViewportChangeSet& changes)
{
    if (playback.phase == phase) {
        return;
    }
    playback.phase = phase;
    changes.playbackPhase = true;
}

void stageBuiltInSecondaryPayload(RequestState& request, DisplayState& display)
{
    if (!request.roles[1].sequence || request.roles[1].provider
        || request.roles[1].activeRequest.target.frame < 0) {
        return;
    }
    PreparedPayload payload;
    payload.commitPending = true;
    payload.generation = request.sequenceGeneration;
    payload.requestId = request.roles[0].activeRequest.identity.id;
    payload.payloadId = ++display.nextPreparedPayloadId;
    request.roles[1].activeRequest.preparedPayloadId = payload.payloadId;
    display.roles[1].pendingRenderPayload = FramePreparation::admitBuiltInFrame(
        request.roles[1].source, request.roles[1].activeRequest.target.frame, payload)
                                                .preparedPayload;
}
}

ImageViewportInternal::ViewportChangeSet ViewportEngineProviderFrameReadyAccess::recordTerminal(
    ViewportEngineProviderTerminalProjectionInput input)
{
    ViewportEngineProviderTerminalProjectionAccess access(m_request);
    return reduceViewportEngineProviderTerminalProjection(std::move(input), std::move(access));
}

ViewportEngineProviderFrameReadyReduction reduceViewportEngineProviderFrameReady(
    ViewportEngineProviderFrameReadyInput input, ViewportEngineProviderFrameReadyAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEngineProviderFrameReadyReduction result;
    if (terminalMatchesActiveRequest(access.m_request)
        || !providerPresent(access.m_request, input.role)
        || !access.m_provider.session.sessionActive
        || !activeTokenMatches(access.m_provider, access.m_request, input.role, input.token)) {
        return result;
    }

    if (input.role == ImageViewportPageRole::Secondary) {
        auto& preparedPayload = access.m_display.roles[0].pendingRenderPayload;
        auto& primaryRequest = access.m_request.roles[0].activeRequest;
        if (!preparedPayload.identity().isValid()) {
            preparedPayload.commitPending = true;
            preparedPayload.generation = access.m_request.sequenceGeneration;
            preparedPayload.requestId = primaryRequest.identity.id;
            preparedPayload.payloadId = ++access.m_display.nextPreparedPayloadId;
            if (displayedPrimaryPayloadMatchesActiveTarget(access.m_display, access.m_request)) {
                preparedPayload.image = access.m_display.roles[0].displayedImage;
            }
            primaryRequest.preparedPayloadId = preparedPayload.payloadId;
        }
        access.m_provider.requests.activeFrameToken = {};
    }

    const auto frameState = preparationState(
        access.m_request, access.m_display, access.m_provider, access.m_presentation, input.role);
    const auto admission = FramePreparation::admitProviderFrame(
        input.frame, input.metadata, input.envelope, frameState);
    if (!admission.accepted()) {
        clearQueue(access.m_provider.requests);
        access.m_provider.requests.activeFrameToken = {};
        result.changes = access.recordTerminal({ input.role, admission.status, admission.reason,
            FailureScope::DisplayRequest, admission.diagnostic, result.changes });
        updatePlaybackPhase(
            access.m_playback, ImageViewport::PlaybackPhase::Stopped, result.changes);
        return result;
    }

    const auto oldRequestStatus = access.m_request.status;
    const auto oldRequestReason = access.m_request.reason;
    const auto oldGeometry = projectViewportGeometryState(input.geometry, access.m_presentation);
    const bool diagnosticsChanged = access.m_request.clearDiagnostics();
    access.m_provider.requests.activeFrameToken = {};

    if (input.role == ImageViewportPageRole::Secondary) {
        access.m_display.roles[1].pendingRenderPayload = admission.preparedPayload;
        const bool primaryReady = access.m_display.roles[0].pendingRenderPayload.commitPending
            && !access.m_display.roles[0].pendingRenderPayload.image.isNull();
        TargetSpreadWaitState wait;
        wait.requiresSecondary = true;
        if (primaryReady && input.geometry.itemBounds.isEmpty()) {
            wait.primary.renderWaiting = true;
            wait.secondary.renderWaiting = true;
        } else if (primaryReady) {
            wait.primary.uploadPending = true;
            wait.secondary.uploadPending = true;
        } else {
            wait.primary.providerWaiting = true;
            wait.secondary.uploadPending = true;
        }
        access.m_request.status = ImageViewport::RequestStatus::Loading;
        access.m_request.reason = projectWaitReason(wait);
        access.m_display.status = retainedDisplayStatus(access.m_display);
    } else {
        access.m_request.targetSpreadTerminal.clear();
        access.m_display.captureRenderFailureRetainedDisplay(
            access.m_request.roles[0].source.facts.present);
        access.m_display.commitPreparedPayloadIdentity(
            access.m_request.roles[0].activeRequest, admission.preparedPayload);
        stageBuiltInSecondaryPayload(access.m_request, access.m_display);
        TargetSpreadWaitState wait;
        wait.requiresSecondary = access.m_request.roles[1].sequence
            && (access.m_request.roles[1].provider
                || access.m_request.roles[1].activeRequest.target.frame >= 0);
        if (input.geometry.itemBounds.isEmpty()) {
            wait.primary.renderWaiting = true;
            if (wait.requiresSecondary && !access.m_request.roles[1].provider) {
                wait.secondary.renderWaiting = true;
            }
        } else {
            wait.primary.uploadPending = true;
            if (wait.requiresSecondary && !access.m_request.roles[1].provider) {
                wait.secondary.uploadPending = true;
            }
        }
        if (wait.requiresSecondary && access.m_request.roles[1].provider
            && access.m_display.roles[1].pendingRenderPayload.image.isNull()) {
            wait.secondary.providerWaiting = true;
        }
        access.m_request.status = ImageViewport::RequestStatus::Loading;
        access.m_request.reason = projectWaitReason(wait);
        access.m_display.status = retainedDisplayStatus(access.m_display);
        access.m_display.roles[0].pendingRenderPayload.commitPending = true;
        if (access.m_playback.phase == ImageViewport::PlaybackPhase::Waiting
            && access.m_request.status == ImageViewport::RequestStatus::Ready
            && !access.m_display.roles[0].pendingRenderPayload.commitPending) {
            updatePlaybackPhase(access.m_playback,
                access.m_playback.stopWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                       : ImageViewport::PlaybackPhase::Playing,
                result.changes);
            access.m_playback.stopWhenRequestReady = false;
        }
    }

    const auto newGeometry = projectViewportGeometryState(input.geometry, access.m_presentation);
    result.changes.requestRevision = oldRequestStatus != access.m_request.status
        || oldRequestReason != access.m_request.reason;
    result.changes.requestState = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.geometryState = PresentationGeometry::contentRect(oldGeometry)
            != PresentationGeometry::contentRect(newGeometry)
        || PresentationGeometry::visibleImageRect(oldGeometry)
            != PresentationGeometry::visibleImageRect(newGeometry);
    result.changes.diagnostics = diagnosticsChanged;
    result.changes.scheduleUpdate = true;
    return result;
}
