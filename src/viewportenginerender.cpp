#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"

#include "imagesequencesource_p.h"
#include "viewportgeometryhelpers_p.h"

namespace {
using namespace ImageViewportInternal;

bool hasSecondary(const RequestState& request)
{
    return request.roles[1].sequence && request.roles[1].activeRequest.target.frame >= 0;
}

bool hasDisplayable(const RequestState& request) { return request.roles[0].source.facts.present; }

bool terminalSealed(const RequestState& request)
{
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.roles[0].activeRequest.identity.id;
}

bool waitingForRender(const RequestState& request)
{
    return request.status == ImageViewport::RequestStatus::Loading
        && (request.reason == ImageViewport::RequestReason::UploadPending
            || request.reason == ImageViewport::RequestReason::RenderWaiting);
}

bool payloadMatches(PreparedPayloadIdentity actual, PreparedPayloadIdentity expected)
{
    return actual.isValid() && expected.isValid() && actual.generation == expected.generation
        && actual.requestId == expected.requestId && actual.payloadId == expected.payloadId;
}

PreparedPayloadIdentity acknowledgedPayload(
    const ViewportRenderAcknowledgement& acknowledgement, ImageViewport::PageRole role)
{
    for (const auto& payload : acknowledgement.rolePayloads) {
        if (payload.role == role) {
            return payload.preparedPayload;
        }
    }
    return role == ImageViewport::PageRole::Primary ? acknowledgement.preparedPayload
                                                    : PreparedPayloadIdentity {};
}

PreparedPayloadIdentity expectedPayload(
    const DisplayState& display, const RequestState& request, ImageViewport::PageRole role)
{
    if (role == ImageViewport::PageRole::Primary) {
        return display.roles[0].pendingRenderPayload.identity();
    }
    if (!hasSecondary(request)) {
        return {};
    }
    const auto secondary = display.roles[1].pendingRenderPayload.identity();
    return secondary.isValid() ? secondary : display.roles[0].pendingRenderPayload.identity();
}

bool primaryAcknowledgementMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    const auto actual = acknowledgedPayload(acknowledgement, ImageViewport::PageRole::Primary);
    return display.roles[0].pendingRenderPayload.commitPending
        && payloadMatches(actual, display.roles[0].pendingRenderPayload.identity())
        && request.activeRequestOwnsPreparedPayload(actual);
}

bool completeAcknowledgementMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    return primaryAcknowledgementMatches(display, request, acknowledgement)
        && (!hasSecondary(request)
            || payloadMatches(
                acknowledgedPayload(acknowledgement, ImageViewport::PageRole::Secondary),
                expectedPayload(display, request, ImageViewport::PageRole::Secondary)));
}

bool failureAcknowledgementMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    if (acknowledgement.failedRole == ImageViewport::PageRole::Primary) {
        return primaryAcknowledgementMatches(display, request, acknowledgement);
    }
    return hasSecondary(request)
        && payloadMatches(acknowledgedPayload(acknowledgement, acknowledgement.failedRole),
            expectedPayload(display, request, acknowledgement.failedRole));
}

bool pendingSpreadReady(const DisplayState& display, const RequestState& request)
{
    return display.roles[0].pendingRenderPayload.commitPending
        && !display.roles[0].pendingRenderPayload.image.isNull()
        && (!hasSecondary(request) || !display.roles[1].pendingRenderPayload.image.isNull());
}

void publishSecondaryDisplay(RequestState& request, DisplayState& display,
    const ProviderGenerationState& secondaryProvider)
{
    if (!hasSecondary(request)) {
        display.roles[1].displayedRequest = {};
        display.roles[1].displayedImageSize = {};
        display.roles[1].displayedImage = {};
        display.roles[1].displayedPayload = {};
        return;
    }
    display.roles[1].displayedRequest.generation = request.sequenceGeneration;
    display.roles[1].displayedRequest.request = request.roles[1].activeRequest;
    const int position = request.roles[1].activeRequest.resolvedFrame.position;
    display.roles[1].displayedRequest.request.target.position = position;
    display.roles[1].displayedRequest.request.resolvedFrame.position = position;
    display.roles[1].displayedImageSize = request.roles[1].provider
        ? secondaryProvider.logicalSize
        : sourceLogicalSize(request.roles[1].source);
    if (!request.roles[1].provider
        && !display.roles[1].pendingRenderPayload.image.isNull()) {
        display.roles[1].displayedImage = display.roles[1].pendingRenderPayload.image;
        display.roles[1].displayedPayload = display.roles[1].pendingRenderPayload;
    }
}

void publishReady(RequestState& request, DisplayState& display,
    const ProviderGenerationState& primaryProvider,
    const ProviderGenerationState& secondaryProvider, const PreparedPayload& payload)
{
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    display.status = ImageViewport::DisplayStatus::Ready;
    if (request.roles[0].source.facts.provider) {
        display.commitPreparedPayloadIdentity(request.roles[0].activeRequest, payload);
    }
    const int frame = request.roles[0].activeRequest.resolvedFrame.frame;
    const int position = request.roles[0].activeRequest.resolvedFrame.position >= 0
        ? request.roles[0].activeRequest.resolvedFrame.position
        : request.roles[0].source.facts.provider
        ? primaryProvider.timingIntervals.frameStartPosition(frame)
        : sourceFrameStartPosition(request.roles[0].source, frame);
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, position);
    display.roles[0].displayedImageSize = request.roles[0].source.facts.provider
        ? primaryProvider.logicalSize
        : sourceLogicalSize(request.roles[0].source);
    display.roles[0].displayedImage = payload.image;
    display.roles[0].displayedPayload = payload;
    publishSecondaryDisplay(request, display, secondaryProvider);
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

void markPlayback(
    ViewportChangeSet& changes, RequestState& request, ImageViewport::PlaybackPhase phase)
{
    if (request.playbackPhase != phase) {
        request.playbackPhase = phase;
        changes.playbackPhase = true;
        changes.requestState = true;
        changes.requestRevision = true;
    }
}
}

ViewportRenderSynchronization ViewportEngine::beginRenderSynchronization(
    const RenderSynchronizationInput& input)
{
    ViewportRenderSynchronization result;
    result.attempt = ++renderAccess().nextSynchronizationAttempt;
    result.oldContentRect = input.oldContentRect;
    result.oldVisibleImageRect = input.oldVisibleImageRect;
    result.oldDisplayStatus = renderAccess().display.status;
    result.pendingTargetCommit = !terminalSealed(renderAccess().request) && waitingForRender(renderAccess().request)
        && pendingSpreadReady(renderAccess().display, renderAccess().request) && !input.itemBounds.isEmpty();
    result.pendingSecondaryProviderCommit = result.pendingTargetCommit
        && hasSecondary(renderAccess().request) && renderAccess().request.roles[1].provider
        && !renderAccess().display.roles[1].pendingRenderPayload.image.isNull();
    if (result.pendingTargetCommit) {
        result.preparedPayload = renderAccess().display.roles[0].pendingRenderPayload;
    } else if (renderAccess().display.roles[0].pendingRenderPayload.commitPending
        && renderAccess().display.hasReadyDisplay(hasDisplayable(renderAccess().request))) {
        result.preparedPayload = renderAccess().display.roles[0].pendingRenderPayload;
        result.preparedPayload.image = renderAccess().display.roles[0].displayedImage;
    }
    result.geometryState
        = geometryState(result.pendingTargetCommit ? input.pendingGeometry : input.currentGeometry);
    result.renderSnapshot = renderSnapshot({ input.itemSize, result.pendingTargetCommit,
        result.preparedPayload, result.geometryState });
    renderAccess().lastSynchronization = result;
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::acknowledgeRenderCommit(
    const RenderAcknowledgementInput& input)
{
    ViewportChangeSet changes;
    if (terminalSealed(renderAccess().request) || !input.renderedImagePresent
        || (input.acknowledgement.synchronizationAttempt != 0
            && (input.acknowledgement.synchronizationAttempt != input.synchronizationAttempt
                || input.synchronizationAttempt != renderAccess().nextSynchronizationAttempt))
        || !completeAcknowledgementMatches(renderAccess().display, renderAccess().request, input.acknowledgement)) {
        return changes;
    }
    const auto oldStatus = renderAccess().display.status;
    if (input.pendingTargetCommit) {
        publishReady(renderAccess().request, renderAccess().display,
            providerState(ImageViewport::PageRole::Primary),
            providerState(ImageViewport::PageRole::Secondary), input.preparedPayload);
    }
    if (input.pendingSecondaryProviderCommit) {
        renderAccess().display.roles[1].displayedImage = renderAccess().display.roles[1].pendingRenderPayload.image;
        renderAccess().display.roles[1].displayedPayload = renderAccess().display.roles[1].pendingRenderPayload;
        renderAccess().display.roles[1].displayedImageSize
            = providerState(ImageViewport::PageRole::Secondary).logicalSize;
    }
    const bool resume = renderAccess().request.playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && renderAccess().request.status == ImageViewport::RequestStatus::Ready;
    renderAccess().display.commitDisplayedRequestSnapshot(renderAccess().request.sequenceGeneration,
        renderAccess().request.roles[0].activeRequest, renderAccess().display.roles[0].pendingRenderPayload.payloadId);
    renderAccess().display.clearPendingRenderPayload();
    renderAccess().display.clearRenderFailureRetainedDisplay();
    if (resume) {
        markPlayback(changes, renderAccess().request,
            renderAccess().request.stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                        : ImageViewport::PlaybackPhase::Playing);
        renderAccess().request.stopPlaybackWhenRequestReady = false;
    }
    if (input.pendingTargetCommit) {
        changes.requestState = true;
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.displayState = renderAccess().display.status != oldStatus;
        changes.geometryState
            = rectsDifferExactly(
                  PresentationGeometry::contentRect(input.geometryState), input.oldContentRect)
            || rectsDifferExactly(PresentationGeometry::visibleImageRect(input.geometryState),
                input.oldVisibleImageRect);
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::acknowledgeRenderFailure(
    const RenderAcknowledgementInput& input)
{
    ViewportChangeSet changes;
    const bool pending
        = waitingForRender(renderAccess().request) && pendingSpreadReady(renderAccess().display, renderAccess().request);
    if (terminalSealed(renderAccess().request)
        || (input.acknowledgement.synchronizationAttempt != 0
            && input.acknowledgement.synchronizationAttempt != renderAccess().nextSynchronizationAttempt)
        || !failureAcknowledgementMatches(renderAccess().display, renderAccess().request, input.acknowledgement)
        || (renderAccess().display.status != ImageViewport::DisplayStatus::Ready && !pending)) {
        return changes;
    }
    const auto oldStatus = renderAccess().display.status;
    const auto failed
        = acknowledgedPayload(input.acknowledgement, input.acknowledgement.failedRole);
    const RenderFailureDiagnostic diagnostic { true, input.acknowledgement.failedRole,
        failed.generation, failed.requestId, failed.payloadId, input.acknowledgement.failureCause };
    renderAccess().request.lastAcceptedRenderFailure = diagnostic;
    changes.renderFailureDiagnostic = diagnostic;
    renderAccess().display.clearPendingRenderPayload();
    if (renderAccess().display.roles[0].retainedDisplayValid) {
        renderAccess().display.status = ImageViewport::DisplayStatus::Retained;
        for (auto& role : renderAccess().display.roles) {
            if (role.retainedDisplayValid) {
                role.displayedRequest = role.retainedRequest;
                role.displayedImageSize = role.retainedImageSize;
                role.displayedImage = role.retainedImage;
            } else {
                role.displayedRequest = {};
                role.displayedImageSize = {};
                role.displayedImage = {};
                role.displayedPayload = {};
            }
        }
    } else {
        renderAccess().display.status = ImageViewport::DisplayStatus::Empty;
        renderAccess().display.clearDisplayedDisplay();
    }
    renderAccess().display.clearRenderFailureRetainedDisplay();
    auto& terminal = renderAccess().request.targetSpreadTerminal;
    terminal.clear();
    terminal.sealed = true;
    terminal.generation = renderAccess().request.sequenceGeneration;
    terminal.requestId = renderAccess().request.roles[0].activeRequest.identity.id;
    auto& role = input.acknowledgement.failedRole == ImageViewport::PageRole::Primary
        ? terminal.primary
        : terminal.secondary;
    role.terminal = true;
    role.status = ImageViewport::RequestStatus::Error;
    role.reason = ImageViewport::RequestReason::RenderFailure;
    role.failureScope = FailureScope::DisplayRequest;
    role.diagnostic = QStringLiteral("render commit failed");
    renderAccess().request.status = role.status;
    renderAccess().request.reason = role.reason;
    const bool diagnosticChanged = renderAccess().request.errorString != role.diagnostic;
    renderAccess().request.errorString = role.diagnostic;
    markPlayback(changes, renderAccess().request, ImageViewport::PlaybackPhase::Stopped);
    changes.requestState = true;
    changes.requestRevision = true;
    changes.diagnostics = diagnosticChanged;
    changes.displayRevision = true;
    changes.displayState = renderAccess().display.status != oldStatus;
    changes.geometryState = rectsDifferExactly(PresentationGeometry::contentRect(
                                                   renderAccess().lastSynchronization.geometryState),
                                renderAccess().lastSynchronization.oldContentRect)
        || rectsDifferExactly(
            PresentationGeometry::visibleImageRect(renderAccess().lastSynchronization.geometryState),
            renderAccess().lastSynchronization.oldVisibleImageRect);
    return changes;
}

ViewportEngine::GeometryChangeResult ViewportEngine::handleGeometryChanged(
    const GeometryChangeInput& input)
{
    GeometryChangeResult result;
    auto& changes = result.changes;
    const GeometryInput demandGeometry { input.geometryState.hasReadyDisplay,
        input.geometryState.itemBounds, input.geometryState.primaryImageSize,
        input.geometryState.secondaryImageSize, input.geometryState.devicePixelRatio };
    if (hasDisplayable(renderAccess().request) && waitingForRender(renderAccess().request)
        && !input.itemBounds.isEmpty()) {
        if (pendingSpreadReady(renderAccess().display, renderAccess().request)) {
            changes.scheduleUpdate = true;
            result.providerEffects = restageProviderDemands(demandGeometry);
            return result;
        }
        if (!renderAccess().request.roles[0].source.facts.provider) {
            stageBuiltIn(renderAccess().request, renderAccess().display);
            renderAccess().request.status = ImageViewport::RequestStatus::Loading;
            renderAccess().request.reason = ImageViewport::RequestReason::UploadPending;
            renderAccess().display.status = renderAccess().display.hasReadyDisplay(hasDisplayable(renderAccess().request))
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
    } else if (renderAccess().request.roles[0].source.facts.provider
        && renderAccess().request.status == ImageViewport::RequestStatus::Loading
        && renderAccess().request.reason == ImageViewport::RequestReason::UploadPending
        && input.itemBounds.isEmpty() && !renderAccess().display.roles[0].pendingRenderPayload.image.isNull()) {
        renderAccess().request.reason = ImageViewport::RequestReason::RenderWaiting;
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
