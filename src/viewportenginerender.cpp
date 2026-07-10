#include "viewportengine_p.h"

#include "imagesequencesource_p.h"
#include "viewportgeometryhelpers_p.h"

namespace {
using namespace ImageViewportInternal;

bool hasSecondary(const RequestState& request)
{
    return request.secondarySequence && request.secondaryActiveRequest.target.frame >= 0;
}

bool hasDisplayable(const RequestState& request) { return request.sequenceSource.facts.present; }

bool terminalSealed(const RequestState& request)
{
    return request.targetSpreadTerminal.sealed
        && request.targetSpreadTerminal.generation == request.sequenceGeneration
        && request.targetSpreadTerminal.requestId == request.activeRequest.identity.id;
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
        return display.pendingRenderPayload.identity();
    }
    if (!hasSecondary(request)) {
        return {};
    }
    const auto secondary = display.secondaryPendingRenderPayload.identity();
    return secondary.isValid() ? secondary : display.pendingRenderPayload.identity();
}

bool primaryAcknowledgementMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    const auto actual = acknowledgedPayload(acknowledgement, ImageViewport::PageRole::Primary);
    return display.pendingRenderPayload.commitPending
        && payloadMatches(actual, display.pendingRenderPayload.identity())
        && request.activeRequestOwnsPreparedPayload(actual);
}

bool completeAcknowledgementMatches(const DisplayState& display, const RequestState& request,
    const ViewportRenderAcknowledgement& acknowledgement)
{
    return primaryAcknowledgementMatches(display, request, acknowledgement)
        && (!hasSecondary(request)
            || payloadMatches(acknowledgedPayload(
                                  acknowledgement, ImageViewport::PageRole::Secondary),
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
    return display.pendingRenderPayload.commitPending
        && !display.pendingRenderPayload.image.isNull()
        && (!hasSecondary(request) || !display.secondaryPendingRenderPayload.image.isNull());
}

void publishSecondaryDisplay(ViewportEngine& engine)
{
    auto& request = engine.requestState();
    auto& display = engine.displayState();
    if (!hasSecondary(request)) {
        display.secondaryDisplayedRequest = {};
        display.secondaryDisplayedImageSize = {};
        display.secondaryDisplayedImage = {};
        return;
    }
    display.secondaryDisplayedRequest.generation = request.sequenceGeneration;
    display.secondaryDisplayedRequest.request = request.secondaryActiveRequest;
    const int position = request.secondaryActiveRequest.resolvedFrame.position;
    display.secondaryDisplayedRequest.request.target.position = position;
    display.secondaryDisplayedRequest.request.resolvedFrame.position = position;
    display.secondaryDisplayedImageSize = request.secondarySequenceIsProvider
        ? engine.secondaryProviderState().logicalSize
        : sourceLogicalSize(request.secondarySequenceSource);
    if (!request.secondarySequenceIsProvider
        && !display.secondaryPendingRenderPayload.image.isNull()) {
        display.secondaryDisplayedImage = display.secondaryPendingRenderPayload.image;
    }
}

void publishReady(ViewportEngine& engine, const PreparedPayload& payload)
{
    auto& request = engine.requestState();
    auto& display = engine.displayState();
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    display.status = ImageViewport::DisplayStatus::Ready;
    if (request.sequenceSource.facts.provider) {
        display.commitPreparedPayloadIdentity(request.activeRequest, payload);
    }
    const int frame = request.activeRequest.resolvedFrame.frame;
    const int position = request.activeRequest.resolvedFrame.position >= 0
        ? request.activeRequest.resolvedFrame.position
        : request.sequenceSource.facts.provider
        ? engine.providerState().timingIntervals.frameStartPosition(frame)
        : sourceFrameStartPosition(request.sequenceSource, frame);
    display.displayedRequest
        = display.activeRequestSnapshot(request.sequenceGeneration, request.activeRequest, position);
    display.displayedImageSize = request.sequenceSource.facts.provider
        ? engine.providerState().logicalSize
        : sourceLogicalSize(request.sequenceSource);
    display.displayedImage = payload.image;
    publishSecondaryDisplay(engine);
}

void stageBuiltIn(ViewportEngine& engine)
{
    auto& request = engine.requestState();
    auto& display = engine.displayState();
    display.captureRenderFailureRetainedDisplay(hasDisplayable(request));
    display.pendingRenderPayload.commitPending = true;
    display.beginPreparedPayloadIdentity(request.sequenceGeneration, request.activeRequest);
    if (request.activeRequest.target.frame >= 0) {
        display.pendingRenderPayload = FramePreparation::admitBuiltInFrame(request.sequenceSource,
            request.activeRequest.target.frame, display.pendingRenderPayload).preparedPayload;
    }
    if (hasSecondary(request) && !request.secondarySequenceIsProvider) {
        auto& secondary = display.secondaryPendingRenderPayload;
        secondary.commitPending = true;
        secondary.generation = request.sequenceGeneration;
        secondary.requestId = request.activeRequest.identity.id;
        secondary.payloadId = ++display.nextPreparedPayloadId;
        request.secondaryActiveRequest.preparedPayloadId = secondary.payloadId;
        secondary = FramePreparation::admitBuiltInFrame(request.secondarySequenceSource,
            request.secondaryActiveRequest.target.frame, secondary).preparedPayload;
    }
}

void markPlayback(ViewportChangeSet& changes, RequestState& request,
    ImageViewport::PlaybackPhase phase)
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
    result.pendingSecondaryProviderCommit = result.pendingTargetCommit && hasSecondary(m_requestState)
        && m_requestState.secondarySequenceIsProvider
        && !m_displayState.secondaryPendingRenderPayload.image.isNull();
    if (result.pendingTargetCommit) {
        result.preparedPayload = m_displayState.pendingRenderPayload;
    } else if (m_displayState.pendingRenderPayload.commitPending
        && m_displayState.hasReadyDisplay(hasDisplayable(m_requestState))) {
        result.preparedPayload = m_displayState.pendingRenderPayload;
        result.preparedPayload.image = m_displayState.displayedImage;
    }
    result.geometryState = geometryState(
        result.pendingTargetCommit ? input.pendingGeometry : input.currentGeometry);
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
        || !completeAcknowledgementMatches(
            m_displayState, m_requestState, input.acknowledgement)) {
        return changes;
    }
    const auto oldStatus = m_displayState.status;
    if (input.pendingTargetCommit) {
        publishReady(*this, input.preparedPayload);
    }
    if (input.pendingSecondaryProviderCommit) {
        m_displayState.secondaryDisplayedImage
            = m_displayState.secondaryPendingRenderPayload.image;
        m_displayState.secondaryDisplayedImageSize = m_secondaryProviderState.logicalSize;
    }
    const bool resume = m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Waiting
        && m_requestState.status == ImageViewport::RequestStatus::Ready;
    m_displayState.commitDisplayedRequestSnapshot(m_requestState.sequenceGeneration,
        m_requestState.activeRequest, m_displayState.pendingRenderPayload.payloadId);
    m_displayState.clearPendingRenderPayload();
    m_displayState.clearRenderFailureRetainedDisplay();
    if (resume) {
        markPlayback(changes, m_requestState, m_requestState.stopPlaybackWhenRequestReady
                ? ImageViewport::PlaybackPhase::Stopped
                : ImageViewport::PlaybackPhase::Playing);
        m_requestState.stopPlaybackWhenRequestReady = false;
    }
    if (input.pendingTargetCommit) {
        changes.requestState = true;
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.displayState = m_displayState.status != oldStatus;
        changes.geometryState = rectsDifferExactly(
                                    PresentationGeometry::contentRect(input.geometryState),
                                    input.oldContentRect)
            || rectsDifferExactly(PresentationGeometry::visibleImageRect(input.geometryState),
                input.oldVisibleImageRect);
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::acknowledgeRenderFailure(
    const RenderAcknowledgementInput& input)
{
    ViewportChangeSet changes;
    const bool pending = waitingForRender(m_requestState)
        && pendingSpreadReady(m_displayState, m_requestState);
    if (terminalSealed(m_requestState)
        || (input.acknowledgement.synchronizationAttempt != 0
            && input.acknowledgement.synchronizationAttempt
                != m_nextRenderSynchronizationAttempt)
        || !failureAcknowledgementMatches(m_displayState, m_requestState, input.acknowledgement)
        || (m_displayState.status != ImageViewport::DisplayStatus::Ready && !pending)) {
        return changes;
    }
    const auto oldStatus = m_displayState.status;
    const auto failed = acknowledgedPayload(
        input.acknowledgement, input.acknowledgement.failedRole);
    const RenderFailureDiagnostic diagnostic { true, input.acknowledgement.failedRole,
        failed.generation, failed.requestId, failed.payloadId,
        input.acknowledgement.failureCause };
    m_requestState.lastAcceptedRenderFailure = diagnostic;
    changes.renderFailureDiagnostic = diagnostic;
    m_displayState.clearPendingRenderPayload();
    if (m_displayState.renderFailureRetainedDisplayValid) {
        m_displayState.status = ImageViewport::DisplayStatus::Retained;
        m_displayState.displayedRequest = m_displayState.renderFailureRetainedRequest;
        m_displayState.displayedImageSize = m_displayState.renderFailureRetainedImageSize;
        m_displayState.displayedImage = m_displayState.renderFailureRetainedImage;
    } else {
        m_displayState.status = ImageViewport::DisplayStatus::Empty;
        m_displayState.clearDisplayedDisplay();
    }
    m_displayState.clearRenderFailureRetainedDisplay();
    auto& terminal = m_requestState.targetSpreadTerminal;
    terminal.clear();
    terminal.sealed = true;
    terminal.generation = m_requestState.sequenceGeneration;
    terminal.requestId = m_requestState.activeRequest.identity.id;
    auto& role = input.acknowledgement.failedRole == ImageViewport::PageRole::Primary
        ? terminal.primary : terminal.secondary;
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
    changes.geometryState = rectsDifferExactly(
                                PresentationGeometry::contentRect(
                                    m_lastRenderSynchronization.geometryState),
                                m_lastRenderSynchronization.oldContentRect)
        || rectsDifferExactly(PresentationGeometry::visibleImageRect(
                                  m_lastRenderSynchronization.geometryState),
            m_lastRenderSynchronization.oldVisibleImageRect);
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::handleGeometryChanged(
    const GeometryChangeInput& input)
{
    ViewportChangeSet changes;
    if (hasDisplayable(m_requestState) && waitingForRender(m_requestState)
        && !input.itemBounds.isEmpty()) {
        if (pendingSpreadReady(m_displayState, m_requestState)) {
            changes.scheduleUpdate = true;
            return changes;
        }
        if (!m_requestState.sequenceSource.facts.provider) {
            stageBuiltIn(*this);
            m_requestState.status = ImageViewport::RequestStatus::Loading;
            m_requestState.reason = ImageViewport::RequestReason::UploadPending;
            m_displayState.status = m_displayState.hasReadyDisplay(hasDisplayable(m_requestState))
                ? ImageViewport::DisplayStatus::Retained : ImageViewport::DisplayStatus::Empty;
            changes.requestState = true;
            changes.requestRevision = true;
            changes.displayState = true;
            changes.displayRevision = true;
            changes.scheduleUpdate = true;
            return changes;
        }
    } else if (m_requestState.sequenceSource.facts.provider
        && m_requestState.status == ImageViewport::RequestStatus::Loading
        && m_requestState.reason == ImageViewport::RequestReason::UploadPending
        && input.itemBounds.isEmpty() && !m_displayState.pendingRenderPayload.image.isNull()) {
        m_requestState.reason = ImageViewport::RequestReason::RenderWaiting;
        changes.requestState = true;
        changes.requestRevision = true;
        changes.displayRevision = true;
    } else {
        changes.displayRevision = true;
    }
    changes.geometryState = rectsDifferExactly(
                                PresentationGeometry::contentRect(input.geometryState),
                                input.oldContentRect)
        || rectsDifferExactly(PresentationGeometry::visibleImageRect(input.geometryState),
            input.oldVisibleImageRect);
    changes.scheduleUpdate = true;
    return changes;
}
