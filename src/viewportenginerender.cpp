#include "viewportengine_p.h"

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

void publishSecondaryDisplay(ViewportEngine& engine)
{
    auto& request = ViewportEngineStateAccess::request(engine);
    auto& display = ViewportEngineStateAccess::display(engine);
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
        ? ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Secondary).logicalSize
        : sourceLogicalSize(request.roles[1].source);
    if (!request.roles[1].provider
        && !display.roles[1].pendingRenderPayload.image.isNull()) {
        display.roles[1].displayedImage = display.roles[1].pendingRenderPayload.image;
        display.roles[1].displayedPayload = display.roles[1].pendingRenderPayload;
    }
}

void publishReady(ViewportEngine& engine, const PreparedPayload& payload)
{
    auto& request = ViewportEngineStateAccess::request(engine);
    auto& display = ViewportEngineStateAccess::display(engine);
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
        ? ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Primary).timingIntervals.frameStartPosition(frame)
        : sourceFrameStartPosition(request.roles[0].source, frame);
    display.roles[0].displayedRequest = display.activeRequestSnapshot(
        request.sequenceGeneration, request.roles[0].activeRequest, position);
    display.roles[0].displayedImageSize = request.roles[0].source.facts.provider
        ? ViewportEngineStateAccess::provider(engine, ImageViewport::PageRole::Primary).logicalSize
        : sourceLogicalSize(request.roles[0].source);
    display.roles[0].displayedImage = payload.image;
    display.roles[0].displayedPayload = payload;
    publishSecondaryDisplay(engine);
}

void stageBuiltIn(ViewportEngine& engine)
{
    auto& request = ViewportEngineStateAccess::request(engine);
    auto& display = ViewportEngineStateAccess::display(engine);
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
    result.attempt = ++m_nextRenderSynchronizationAttempt;
    result.oldContentRect = input.oldContentRect;
    result.oldVisibleImageRect = input.oldVisibleImageRect;
    result.oldDisplayStatus = m_displayState.status;
    result.pendingTargetCommit = !terminalSealed(m_requestState) && waitingForRender(m_requestState)
        && pendingSpreadReady(m_displayState, m_requestState) && !input.itemBounds.isEmpty();
    result.pendingSecondaryProviderCommit = result.pendingTargetCommit
        && hasSecondary(m_requestState) && m_requestState.roles[1].provider
        && !m_displayState.roles[1].pendingRenderPayload.image.isNull();
    if (result.pendingTargetCommit) {
        result.preparedPayload = m_displayState.roles[0].pendingRenderPayload;
    } else if (m_displayState.roles[0].pendingRenderPayload.commitPending
        && m_displayState.hasReadyDisplay(hasDisplayable(m_requestState))) {
        result.preparedPayload = m_displayState.roles[0].pendingRenderPayload;
        result.preparedPayload.image = m_displayState.roles[0].displayedImage;
    }
    result.geometryState
        = geometryState(result.pendingTargetCommit ? input.pendingGeometry : input.currentGeometry);
    result.renderSnapshot = renderSnapshot({ input.itemSize, result.pendingTargetCommit,
        result.preparedPayload, result.geometryState });
    m_lastRenderSynchronization = result;
    return result;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::acknowledgeRenderCommit(
    const RenderAcknowledgementInput& input)
{
    ViewportChangeSet changes;
    if (terminalSealed(m_requestState) || !input.renderedImagePresent
        || (input.acknowledgement.synchronizationAttempt != 0
            && (input.acknowledgement.synchronizationAttempt != input.synchronizationAttempt
                || input.synchronizationAttempt != m_nextRenderSynchronizationAttempt))
        || !completeAcknowledgementMatches(m_displayState, m_requestState, input.acknowledgement)) {
        return changes;
    }
    const auto oldStatus = m_displayState.status;
    if (input.pendingTargetCommit) {
        publishReady(*this, input.preparedPayload);
    }
    if (input.pendingSecondaryProviderCommit) {
        m_displayState.roles[1].displayedImage = m_displayState.roles[1].pendingRenderPayload.image;
        m_displayState.roles[1].displayedPayload = m_displayState.roles[1].pendingRenderPayload;
        m_displayState.roles[1].displayedImageSize = secondaryProviderState().logicalSize;
    }
    const bool resume = m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && m_requestState.status == ImageViewport::RequestStatus::Ready;
    m_displayState.commitDisplayedRequestSnapshot(m_requestState.sequenceGeneration,
        m_requestState.roles[0].activeRequest, m_displayState.roles[0].pendingRenderPayload.payloadId);
    m_displayState.clearPendingRenderPayload();
    m_displayState.clearRenderFailureRetainedDisplay();
    if (resume) {
        markPlayback(changes, m_requestState,
            m_requestState.stopPlaybackWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                        : ImageViewport::PlaybackPhase::Playing);
        m_requestState.stopPlaybackWhenRequestReady = false;
    }
    if (input.pendingTargetCommit) {
        changes.requestState = true;
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.displayState = m_displayState.status != oldStatus;
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
        = waitingForRender(m_requestState) && pendingSpreadReady(m_displayState, m_requestState);
    if (terminalSealed(m_requestState)
        || (input.acknowledgement.synchronizationAttempt != 0
            && input.acknowledgement.synchronizationAttempt != m_nextRenderSynchronizationAttempt)
        || !failureAcknowledgementMatches(m_displayState, m_requestState, input.acknowledgement)
        || (m_displayState.status != ImageViewport::DisplayStatus::Ready && !pending)) {
        return changes;
    }
    const auto oldStatus = m_displayState.status;
    const auto failed
        = acknowledgedPayload(input.acknowledgement, input.acknowledgement.failedRole);
    const RenderFailureDiagnostic diagnostic { true, input.acknowledgement.failedRole,
        failed.generation, failed.requestId, failed.payloadId, input.acknowledgement.failureCause };
    m_requestState.lastAcceptedRenderFailure = diagnostic;
    changes.renderFailureDiagnostic = diagnostic;
    m_displayState.clearPendingRenderPayload();
    if (m_displayState.roles[0].retainedDisplayValid) {
        m_displayState.status = ImageViewport::DisplayStatus::Retained;
        for (auto& role : m_displayState.roles) {
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
        m_displayState.status = ImageViewport::DisplayStatus::Empty;
        m_displayState.clearDisplayedDisplay();
    }
    m_displayState.clearRenderFailureRetainedDisplay();
    auto& terminal = m_requestState.targetSpreadTerminal;
    terminal.clear();
    terminal.sealed = true;
    terminal.generation = m_requestState.sequenceGeneration;
    terminal.requestId = m_requestState.roles[0].activeRequest.identity.id;
    auto& role = input.acknowledgement.failedRole == ImageViewport::PageRole::Primary
        ? terminal.primary
        : terminal.secondary;
    role.terminal = true;
    role.status = ImageViewport::RequestStatus::Error;
    role.reason = ImageViewport::RequestReason::RenderFailure;
    role.failureScope = FailureScope::DisplayRequest;
    role.diagnostic = QStringLiteral("render commit failed");
    m_requestState.status = role.status;
    m_requestState.reason = role.reason;
    const bool diagnosticChanged = m_requestState.errorString != role.diagnostic;
    m_requestState.errorString = role.diagnostic;
    markPlayback(changes, m_requestState, ImageViewport::PlaybackPhase::Stopped);
    changes.requestState = true;
    changes.requestRevision = true;
    changes.diagnostics = diagnosticChanged;
    changes.displayRevision = true;
    changes.displayState = m_displayState.status != oldStatus;
    changes.geometryState = rectsDifferExactly(PresentationGeometry::contentRect(
                                                   m_lastRenderSynchronization.geometryState),
                                m_lastRenderSynchronization.oldContentRect)
        || rectsDifferExactly(
            PresentationGeometry::visibleImageRect(m_lastRenderSynchronization.geometryState),
            m_lastRenderSynchronization.oldVisibleImageRect);
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
    if (hasDisplayable(m_requestState) && waitingForRender(m_requestState)
        && !input.itemBounds.isEmpty()) {
        if (pendingSpreadReady(m_displayState, m_requestState)) {
            changes.scheduleUpdate = true;
            result.providerEffects = restageProviderDemands(demandGeometry);
            return result;
        }
        if (!m_requestState.roles[0].source.facts.provider) {
            stageBuiltIn(*this);
            m_requestState.status = ImageViewport::RequestStatus::Loading;
            m_requestState.reason = ImageViewport::RequestReason::UploadPending;
            m_displayState.status = m_displayState.hasReadyDisplay(hasDisplayable(m_requestState))
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
    } else if (m_requestState.roles[0].source.facts.provider
        && m_requestState.status == ImageViewport::RequestStatus::Loading
        && m_requestState.reason == ImageViewport::RequestReason::UploadPending
        && input.itemBounds.isEmpty() && !m_displayState.roles[0].pendingRenderPayload.image.isNull()) {
        m_requestState.reason = ImageViewport::RequestReason::RenderWaiting;
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
