#include "framepreparation_p.h"
#include "imagesequencesource_p.h"
#include "viewportenginerenderackhelpers_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportgeometryhelpers_p.h"

#include <limits>

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
    return request.status == ImageViewportRequestStatus::Loading
        && (request.reason == ImageViewportRequestReason::UploadPending
            || request.reason == ImageViewportRequestReason::RenderWaiting);
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
    request.status = ImageViewportRequestStatus::Ready;
    request.reason = ImageViewportRequestReason::Ready;
    display.status = ImageViewportDisplayStatus::Ready;
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
    ViewportChangeSet& changes, PlaybackState& playback, ImageViewportPlaybackPhase phase)
{
    if (playback.phase == phase)
        return;
    playback.phase = phase;
    changes.playbackPhase = true;
    changes.requestState = true;
    changes.requestRevision = true;
}
}

ViewportEngineRenderCoordinationState::AttemptContext synchronizeViewportEngineRender(
    ViewportEngineRenderSynchronizationInput input,
    ViewportEngineRenderSynchronizationAccess access)
{
    ViewportEngineRenderCoordinationState::AttemptContext result;
    if (access.render().nextSynchronizationAttempt == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport render attempt identity exhausted");
    }
    result.attempt.attempt = ++access.render().nextSynchronizationAttempt;
    result.oldContentRect = input.oldContentRect;
    result.oldVisibleImageRect = input.oldVisibleImageRect;
    result.oldDisplayStatus = access.display().status;
    result.pendingTargetCommit = !terminalSealed(access.request())
        && waitingForRender(access.request())
        && pendingSpreadReady(access.display(), access.request()) && !input.itemBounds.isEmpty();
    result.pendingPrimaryRefinementCommit = !result.pendingTargetCommit
        && access.request().status == ImageViewportRequestStatus::Ready
        && access.display().status == ImageViewportDisplayStatus::Ready
        && access.display().roles[0].pendingRenderPayload.commitPending
        && !access.display().roles[0].pendingRenderPayload.image.isNull();
    result.pendingSecondaryRefinementCommit = !result.pendingTargetCommit
        && hasSecondary(access.request())
        && access.request().status == ImageViewportRequestStatus::Ready
        && access.display().status == ImageViewportDisplayStatus::Ready
        && access.display().roles[1].pendingRenderPayload.commitPending
        && !access.display().roles[1].pendingRenderPayload.image.isNull();
    result.pendingRefinementCommit
        = (result.pendingPrimaryRefinementCommit || result.pendingSecondaryRefinementCommit)
        && !input.itemBounds.isEmpty();
    result.pendingSecondaryProviderCommit = result.pendingTargetCommit
        && hasSecondary(access.request()) && access.request().roles[1].provider
        && !access.display().roles[1].pendingRenderPayload.image.isNull();
    if (result.pendingTargetCommit || result.pendingPrimaryRefinementCommit)
        result.preparedPayload = access.display().roles[0].pendingRenderPayload;
    else if (access.display().hasReadyDisplay(access.request().roles[0].source.facts.present)) {
        result.preparedPayload = access.display().roles[0].displayedPayload;
        result.preparedPayload.image = access.display().roles[0].displayedImage;
    }
    result.geometryState = projectViewportGeometryState(
        result.pendingTargetCommit ? input.pendingGeometry : input.currentGeometry,
        access.presentation());
    result.attempt.snapshot
        = projectViewportRenderSnapshot({ input.itemSize, result.pendingTargetCommit,
                                            result.preparedPayload, result.geometryState },
            access.renderSnapshot());
    return result;
}

ViewportEngineRenderCommitReduction reduceViewportEngineRenderCommit(
    ViewportEngineRenderAcknowledgementInput input, ViewportEngineRenderCommitAccess access)
{
    ViewportEngineRenderCommitReduction result;
    auto& changes = result.changes;
    if (terminalSealed(access.request()) || !input.renderedImagePresent
        || input.acknowledgement.attempt != input.attempt
        || !ViewportEngineRenderAcknowledgement::completeMatches(
            access.display(), access.request(), input.acknowledgement)) {
        const auto payload = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
            input.acknowledgement, ImageViewportPageRole::Primary);
        ImageViewportInternal::InternalObservation observation;
        observation.subsystem = ImageViewportInternal::InternalObservationSubsystem::Engine;
        observation.category = ImageViewportInternal::InternalObservationCategory::StaleDrop;
        observation.cause
            = ImageViewportInternal::InternalObservationCause::StaleRenderAcknowledgement;
        observation.identity.roleValid = true;
        observation.identity.role = ImageViewportPageRole::Primary;
        observation.identity.generation = payload.generation;
        observation.identity.requestId = payload.requestId;
        observation.identity.payloadId = payload.payloadId;
        observation.identity.renderAttempt = input.acknowledgement.attempt;
        result.observations.append(observation);
        return result;
    }
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
    if (input.pendingRefinementCommit) {
        for (const auto role :
            { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
            const auto index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
            const bool candidate = role == ImageViewportPageRole::Primary
                ? input.pendingPrimaryRefinementCommit
                : input.pendingSecondaryRefinementCommit;
            if (!candidate)
                continue;
            auto& shown = access.display().roles[index];
            shown.displayedImage = shown.pendingRenderPayload.image;
            shown.displayedPayload = shown.pendingRenderPayload;
            shown.displayedImageSize = providerFor(access.providerFacts(), role).logicalSize;
            shown.displayedRequest.generation = access.request().sequenceGeneration;
            shown.displayedRequest.request = access.request().roles[index].activeRequest;
        }
    }
    const bool resume = access.playback().phase == ImageViewportPlaybackPhase::Waiting
        && access.request().status == ImageViewportRequestStatus::Ready;
    if (input.pendingTargetCommit) {
        access.display().commitDisplayedRequestSnapshot(access.request().sequenceGeneration,
            access.request().roles[0].activeRequest,
            access.display().roles[0].pendingRenderPayload.payloadId);
    }
    access.display().clearPendingRenderPayload();
    access.display().clearRenderFailureRetainedDisplay();
    if (resume) {
        markPlayback(changes, access.playback(),
            access.playback().stopWhenRequestReady ? ImageViewportPlaybackPhase::Stopped
                                                   : ImageViewportPlaybackPhase::Playing);
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
    if (input.pendingRefinementCommit) {
        changes.displayRevision = true;
        changes.scheduleUpdate = true;
    }
    return result;
}

ViewportEngineRenderFailureReduction reduceViewportEngineRenderFailure(
    ViewportEngineRenderAcknowledgementInput input, ViewportEngineRenderFailureAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEngineRenderFailureReduction result;
    auto& changes = result.changes;
    if (input.pendingRefinementCommit && input.acknowledgement.attempt == input.attempt) {
        const auto index
            = input.acknowledgement.failedRole == ImageViewportPageRole::Secondary ? 1U : 0U;
        const bool failedRoleIsCandidate = index == 0U ? input.pendingPrimaryRefinementCommit
                                                       : input.pendingSecondaryRefinementCommit;
        if (failedRoleIsCandidate) {
            access.display().roles[index].pendingRenderPayload = {};
        } else {
            if (input.pendingPrimaryRefinementCommit)
                access.display().roles[0].pendingRenderPayload = {};
            if (input.pendingSecondaryRefinementCommit)
                access.display().roles[1].pendingRenderPayload = {};
        }
        const bool siblingPending
            = access.display().roles[1U - index].pendingRenderPayload.commitPending;
        changes.scheduleUpdate = siblingPending;
        return result;
    }
    const bool pending = waitingForRender(access.request())
        && pendingSpreadReady(access.display(), access.request());
    if (terminalSealed(access.request()) || input.acknowledgement.attempt != input.attempt
        || !ViewportEngineRenderAcknowledgement::failureMatches(
            access.display(), access.request(), input.acknowledgement)
        || (access.display().status != ImageViewportDisplayStatus::Ready && !pending)) {
        const auto failed = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
            input.acknowledgement, input.acknowledgement.failedRole);
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::Engine;
        observation.category = InternalObservationCategory::StaleDrop;
        observation.cause = InternalObservationCause::StaleRenderAcknowledgement;
        observation.identity.roleValid = true;
        observation.identity.role = input.acknowledgement.failedRole;
        observation.identity.generation = failed.generation;
        observation.identity.requestId = failed.requestId;
        observation.identity.payloadId = failed.payloadId;
        observation.identity.renderAttempt = input.acknowledgement.attempt;
        observation.detail = int(input.acknowledgement.failureCause);
        result.observations.append(observation);
        return result;
    }
    const auto oldStatus = access.display().status;
    const auto failed = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
        input.acknowledgement, input.acknowledgement.failedRole);
    result.diagnostic
        = { true, input.acknowledgement.failedRole, failed.generation, failed.requestId,
              failed.payloadId, input.acknowledgement.failureCause, input.acknowledgement.attempt };
    access.request().lastAcceptedRenderFailure = result.diagnostic;
    changes.renderFailureDiagnostic = result.diagnostic;
    access.display().clearPendingRenderPayload();
    if (access.display().roles[0].retainedDisplayValid) {
        access.display().status = ImageViewportDisplayStatus::Retained;
        for (auto& role : access.display().roles) {
            if (role.retainedDisplayValid) {
                role.displayedRequest = role.retainedRequest;
                role.displayedImageSize = role.retainedImageSize;
                role.displayedImage = role.retainedImage;
            } else
                role = {};
        }
    } else {
        access.display().status = ImageViewportDisplayStatus::Empty;
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
    role.status = ImageViewportRequestStatus::Error;
    role.reason = ImageViewportRequestReason::RenderFailure;
    role.failureScope = FailureScope::DisplayRequest;
    role.diagnostic = QStringLiteral("render commit failed");
    access.request().status = role.status;
    access.request().reason = role.reason;
    const bool diagnosticChanged = access.request().errorString != role.diagnostic;
    access.request().errorString = role.diagnostic;
    markPlayback(changes, access.playback(), ImageViewportPlaybackPhase::Stopped);
    changes.requestState = true;
    changes.requestRevision = true;
    changes.diagnostics = diagnosticChanged;
    changes.displayRevision = true;
    changes.displayState = access.display().status != oldStatus;
    changes.geometryState
        = rectsDifferExactly(
              PresentationGeometry::contentRect(input.geometryState), input.oldContentRect)
        || rectsDifferExactly(
            PresentationGeometry::visibleImageRect(input.geometryState), input.oldVisibleImageRect);
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
            access.request().status = ImageViewportRequestStatus::Loading;
            access.request().reason = ImageViewportRequestReason::UploadPending;
            access.display().status
                = access.display().hasReadyDisplay(hasDisplayable(access.request()))
                ? ImageViewportDisplayStatus::Retained
                : ImageViewportDisplayStatus::Empty;
            changes.requestState = true;
            changes.requestRevision = true;
            changes.displayState = true;
            changes.displayRevision = true;
            changes.scheduleUpdate = true;
            return result;
        }
    } else if (access.request().roles[0].source.facts.provider
        && access.request().status == ImageViewportRequestStatus::Loading
        && access.request().reason == ImageViewportRequestReason::UploadPending
        && input.itemBounds.isEmpty()
        && !access.display().roles[0].pendingRenderPayload.image.isNull()) {
        access.request().reason = ImageViewportRequestReason::RenderWaiting;
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
