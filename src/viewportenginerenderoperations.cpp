#include "framepreparation_p.h"
#include "imagesequencesource_p.h"
#include "viewportenginerenderackhelpers_p.h"
#include "viewportenginerenderoperations_p.h"
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
    display.beginPreparedPayloadIdentity(
        request.sequenceGeneration, request.roles[0].activeRequest);
    if (request.roles[0].activeRequest.target.frame >= 0) {
        display.roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(request.roles[0].source,
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
const ProviderFactsState& providerFor(
    const ViewportEngineProviderFactsView& facts, ImageViewportPageRole role)
{
    return facts[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}
void publishSecondary(
    RequestState& request, DisplayState& display, const ProviderFactsState& provider)
{
    if (!hasSecondary(request)) {
        display.roles[1] = {};
        return;
    }
    auto& shown = display.roles[1];
    shown.displayedRequest.generation = request.sequenceGeneration;
    shown.displayedRequest.request = request.roles[1].activeRequest;
    const int position = request.roles[1].activeRequest.resolvedFrame.position;
    shown.displayedRequest.request.target.position = position;
    shown.displayedRequest.request.resolvedFrame.position = position;
    shown.displayedImageSize = request.roles[1].provider
        ? provider.logicalSize
        : sourceLogicalSize(request.roles[1].source);
    if (!request.roles[1].provider && !shown.pendingRenderPayload.image.isNull()) {
        shown.displayedImage = shown.pendingRenderPayload.image;
        shown.displayedPayload = shown.pendingRenderPayload;
    }
}
void publishReady(RequestState& request, DisplayState& display,
    const ProviderFactsState& primaryProvider, const ProviderFactsState& secondaryProvider,
    const PreparedPayload& payload)
{
    request.status = ImageViewport::RequestStatus::Ready;
    request.reason = ImageViewport::RequestReason::Ready;
    display.status = ImageViewport::DisplayStatus::Ready;
    if (request.roles[0].source.facts.provider)
        display.commitPreparedPayloadIdentity(request.roles[0].activeRequest, payload);
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
    publishSecondary(request, display, secondaryProvider);
}
void markPlayback(
    ViewportChangeSet& changes, PlaybackState& playback, ImageViewport::PlaybackPhase phase)
{
    if (playback.phase == phase)
        return;
    playback.phase = phase;
    changes.playbackPhase = true;
    changes.requestState = true;
    changes.requestRevision = true;
}
}

ViewportRenderSynchronization synchronizeViewportEngineRender(
    ViewportEngineRenderSynchronizationInput input,
    ViewportEngineRenderSynchronizationAccess access)
{
    ViewportRenderSynchronization result;
    result.attempt = ++access.render().nextSynchronizationAttempt;
    result.oldContentRect = input.oldContentRect;
    result.oldVisibleImageRect = input.oldVisibleImageRect;
    result.oldDisplayStatus = access.display().status;
    result.pendingTargetCommit = !terminalSealed(access.request())
        && waitingForRender(access.request())
        && pendingSpreadReady(access.display(), access.request()) && !input.itemBounds.isEmpty();
    result.pendingSecondaryProviderCommit = result.pendingTargetCommit
        && hasSecondary(access.request()) && access.request().roles[1].provider
        && !access.display().roles[1].pendingRenderPayload.image.isNull();
    if (result.pendingTargetCommit)
        result.preparedPayload = access.display().roles[0].pendingRenderPayload;
    else if (access.display().roles[0].pendingRenderPayload.commitPending
        && access.display().hasReadyDisplay(access.request().roles[0].source.facts.present)) {
        result.preparedPayload = access.display().roles[0].pendingRenderPayload;
        result.preparedPayload.image = access.display().roles[0].displayedImage;
    }
    result.geometryState = projectViewportGeometryState(
        result.pendingTargetCommit ? input.pendingGeometry : input.currentGeometry,
        access.presentation());
    result.renderSnapshot
        = projectViewportRenderSnapshot({ input.itemSize, result.pendingTargetCommit,
                                            result.preparedPayload, result.geometryState },
            access.renderSnapshot());
    access.render().lastSynchronization = result;
    return result;
}

ViewportEngineRenderCommitReduction reduceViewportEngineRenderCommit(
    ViewportEngineRenderAcknowledgementInput input, ViewportEngineRenderCommitAccess access)
{
    ViewportEngineRenderCommitReduction result;
    auto& changes = result.changes;
    if (terminalSealed(access.request()) || !input.renderedImagePresent
        || (input.acknowledgement.synchronizationAttempt != 0
            && (input.acknowledgement.synchronizationAttempt != input.synchronizationAttempt
                || input.synchronizationAttempt != access.render().nextSynchronizationAttempt))
        || !ViewportEngineRenderAcknowledgement::completeMatches(
            access.display(), access.request(), input.acknowledgement))
        return result;
    const auto oldStatus = access.display().status;
    if (input.pendingTargetCommit)
        publishReady(access.request(), access.display(),
            providerFor(access.providerFacts(), ImageViewportPageRole::Primary),
            providerFor(access.providerFacts(), ImageViewportPageRole::Secondary),
            input.preparedPayload);
    if (input.pendingSecondaryProviderCommit) {
        access.display().roles[1].displayedImage
            = access.display().roles[1].pendingRenderPayload.image;
        access.display().roles[1].displayedPayload = access.display().roles[1].pendingRenderPayload;
        access.display().roles[1].displayedImageSize
            = providerFor(access.providerFacts(), ImageViewportPageRole::Secondary).logicalSize;
    }
    const bool resume = access.playback().phase == ImageViewport::PlaybackPhase::Waiting
        && access.request().status == ImageViewport::RequestStatus::Ready;
    access.display().commitDisplayedRequestSnapshot(access.request().sequenceGeneration,
        access.request().roles[0].activeRequest,
        access.display().roles[0].pendingRenderPayload.payloadId);
    access.display().clearPendingRenderPayload();
    access.display().clearRenderFailureRetainedDisplay();
    if (resume) {
        markPlayback(changes, access.playback(),
            access.playback().stopWhenRequestReady ? ImageViewport::PlaybackPhase::Stopped
                                                   : ImageViewport::PlaybackPhase::Playing);
        access.playback().stopWhenRequestReady = false;
    }
    if (input.pendingTargetCommit) {
        changes.requestState = true;
        changes.requestRevision = true;
        changes.displayRevision = true;
        changes.displayState = access.display().status != oldStatus;
        changes.geometryState
            = rectsDifferExactly(
                  PresentationGeometry::contentRect(input.geometryState), input.oldContentRect)
            || rectsDifferExactly(PresentationGeometry::visibleImageRect(input.geometryState),
                input.oldVisibleImageRect);
    }
    return result;
}

ViewportEngineRenderFailureReduction reduceViewportEngineRenderFailure(
    ViewportEngineRenderAcknowledgementInput input, ViewportEngineRenderFailureAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEngineRenderFailureReduction result;
    auto& changes = result.changes;
    const bool pending = waitingForRender(access.request())
        && pendingSpreadReady(access.display(), access.request());
    if (terminalSealed(access.request())
        || (input.acknowledgement.synchronizationAttempt != 0
            && input.acknowledgement.synchronizationAttempt
                != access.render().nextSynchronizationAttempt)
        || !ViewportEngineRenderAcknowledgement::failureMatches(
            access.display(), access.request(), input.acknowledgement)
        || (access.display().status != ImageViewport::DisplayStatus::Ready && !pending))
        return result;
    const auto oldStatus = access.display().status;
    const auto failed = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
        input.acknowledgement, input.acknowledgement.failedRole);
    result.diagnostic = { true, input.acknowledgement.failedRole, failed.generation,
        failed.requestId, failed.payloadId, input.acknowledgement.failureCause };
    access.request().lastAcceptedRenderFailure = result.diagnostic;
    changes.renderFailureDiagnostic = result.diagnostic;
    access.display().clearPendingRenderPayload();
    if (access.display().roles[0].retainedDisplayValid) {
        access.display().status = ImageViewport::DisplayStatus::Retained;
        for (auto& role : access.display().roles) {
            if (role.retainedDisplayValid) {
                role.displayedRequest = role.retainedRequest;
                role.displayedImageSize = role.retainedImageSize;
                role.displayedImage = role.retainedImage;
            } else
                role = {};
        }
    } else {
        access.display().status = ImageViewport::DisplayStatus::Empty;
        access.display().clearDisplayedDisplay();
    }
    access.display().clearRenderFailureRetainedDisplay();
    auto& terminal = access.request().targetSpreadTerminal;
    terminal.clear();
    terminal.sealed = true;
    terminal.generation = access.request().sequenceGeneration;
    terminal.requestId = access.request().roles[0].activeRequest.identity.id;
    auto& role = input.acknowledgement.failedRole == ImageViewportPageRole::Primary
        ? terminal.primary
        : terminal.secondary;
    role.terminal = true;
    role.status = ImageViewport::RequestStatus::Error;
    role.reason = ImageViewport::RequestReason::RenderFailure;
    role.failureScope = FailureScope::DisplayRequest;
    role.diagnostic = QStringLiteral("render commit failed");
    access.request().status = role.status;
    access.request().reason = role.reason;
    const bool diagnosticChanged = access.request().errorString != role.diagnostic;
    access.request().errorString = role.diagnostic;
    markPlayback(changes, access.playback(), ImageViewport::PlaybackPhase::Stopped);
    changes.requestState = true;
    changes.requestRevision = true;
    changes.diagnostics = diagnosticChanged;
    changes.displayRevision = true;
    changes.displayState = access.display().status != oldStatus;
    changes.geometryState
        = rectsDifferExactly(
              PresentationGeometry::contentRect(access.render().lastSynchronization.geometryState),
              access.render().lastSynchronization.oldContentRect)
        || rectsDifferExactly(PresentationGeometry::visibleImageRect(
                                  access.render().lastSynchronization.geometryState),
            access.render().lastSynchronization.oldVisibleImageRect);
    return result;
}

ViewportEngineGeometryChangeReduction reduceViewportEngineGeometryChange(
    ViewportEngineGeometryChangeInput input, ViewportEngineGeometryChangeAccess access)
{
    ViewportEngineGeometryChangeReduction result;
    auto& changes = result.changes;
    result.providerDemandGeometry
        = ViewportEngineGeometryInput { input.geometryState.hasReadyDisplay,
              input.geometryState.itemBounds, input.geometryState.primaryImageSize,
              input.geometryState.secondaryImageSize, input.geometryState.devicePixelRatio };

    if (hasDisplayable(access.request()) && waitingForRender(access.request())
        && !input.itemBounds.isEmpty()) {
        if (pendingSpreadReady(access.display(), access.request())) {
            changes.scheduleUpdate = true;
            return result;
        }
        if (!access.request().roles[0].source.facts.provider) {
            stageBuiltIn(access.request(), access.display());
            access.request().status = ImageViewport::RequestStatus::Loading;
            access.request().reason = ImageViewport::RequestReason::UploadPending;
            access.display().status
                = access.display().hasReadyDisplay(hasDisplayable(access.request()))
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            changes.requestState = true;
            changes.requestRevision = true;
            changes.displayState = true;
            changes.displayRevision = true;
            changes.scheduleUpdate = true;
            return result;
        }
    } else if (access.request().roles[0].source.facts.provider
        && access.request().status == ImageViewport::RequestStatus::Loading
        && access.request().reason == ImageViewport::RequestReason::UploadPending
        && input.itemBounds.isEmpty()
        && !access.display().roles[0].pendingRenderPayload.image.isNull()) {
        access.request().reason = ImageViewport::RequestReason::RenderWaiting;
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
    return result;
}
