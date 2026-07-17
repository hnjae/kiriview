#include "framepreparation_p.h"
#include "imagesequencesource_p.h"
#include "viewportenginebuiltinframeoperations_p.h"
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
ViewportEngineBuiltInFrameStageResult stageBuiltIn(RequestState& request, DisplayState& display,
    ImageViewportExactnessPreference exactnessPreference)
{
    return stageViewportEngineBuiltInTargetSpread(request, display, exactnessPreference);
}
const ProviderFactsState& providerFor(
    ViewportEngineProviderFactsView facts, ImageViewportPageRole role)
{
    return facts[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}
void publishSecondary(RequestState& request, DisplayState& display)
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
    if (!shown.pendingRenderPayload.image.isNull())
        shown.displayedPayload = shown.pendingRenderPayload;
}
void publishReady(RequestState& request, DisplayState& display,
    const ProviderFactsState& primaryProvider, const PreparedPayload& payload)
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
    display.roles[0].displayedPayload = payload;
    publishSecondary(request, display);
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
        && pendingSpreadReady(access.display(), access.request()) && !input.itemBounds.isEmpty()
        && input.currentGeometry.renderAvailable;
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
        && !input.itemBounds.isEmpty() && input.currentGeometry.renderAvailable;
    if (result.pendingTargetCommit || result.pendingPrimaryRefinementCommit)
        result.preparedPayload = access.display().roles[0].pendingRenderPayload;
    else if (access.display().hasReadyDisplay(access.request().roles[0].source.facts.present))
        result.preparedPayload = access.display().roles[0].displayedPayload;
    const auto& renderPresentation = access.display().status == ImageViewportDisplayStatus::Retained
            && !result.pendingTargetCommit
        ? access.display().displayedPresentation
        : access.presentation();
    result.geometryState = projectViewportGeometryState(
        result.pendingTargetCommit ? input.pendingGeometry : input.currentGeometry,
        renderPresentation);
    std::array<PreparedPayload, 2> payloads;
    ImageViewportRoleSet requiredRoles;
    if (result.pendingTargetCommit) {
        requiredRoles = ImageViewportRoleSet(true, hasSecondary(access.request()));
        payloads[0] = access.display().roles[0].pendingRenderPayload;
        if (requiredRoles.secondary())
            payloads[1] = access.display().roles[1].pendingRenderPayload;
    } else if (access.display().hasReadyDisplay(access.request().roles[0].source.facts.present)) {
        const bool secondaryDisplayed
            = access.display().roles[1].displayedPayload.hasPresentableContent();
        requiredRoles = ImageViewportRoleSet(true, secondaryDisplayed);
        for (std::size_t index = 0; index < payloads.size(); ++index)
            payloads[index] = access.display().roles[index].displayedPayload;
        if (result.pendingPrimaryRefinementCommit)
            payloads[0] = access.display().roles[0].pendingRenderPayload;
        if (result.pendingSecondaryRefinementCommit)
            payloads[1] = access.display().roles[1].pendingRenderPayload;
    }
    for (auto& payload : payloads)
        payload.providerFrameLeaseId = 0;
    TargetSpreadIdentity targetSpread;
    if (requiredRoles.primary()) {
        if (result.pendingTargetCommit) {
            targetSpread = access.request().activeTargetSpreadIdentity();
        } else {
            const auto& displayed = access.display().roles[0].displayedRequest;
            targetSpread = { displayed.generation, displayed.request.identity.id };
        }
    }
    const RenderPresentationIdentity presentationIdentity {
        access.display().status == ImageViewportDisplayStatus::Retained
                && !result.pendingTargetCommit
            ? input.displayedPresentationRevision
            : input.targetPresentationRevision,
    };
    result.attempt.snapshot = projectViewportRenderSnapshot(
        { targetSpread, presentationIdentity, input.itemSize, requiredRoles, payloads,
            result.geometryState,
            access.display().status == ImageViewportDisplayStatus::Retained
                && !result.pendingTargetCommit },
        access.renderSnapshot());
    return result;
}

ViewportEngineRenderCommitReduction reduceViewportEngineRenderCommit(
    ViewportEngineRenderAcknowledgementInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineRenderCommitAccess access)
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
        observation.identity.requestId = input.acknowledgement.targetSpread.requestId;
        observation.identity.payloadId = payload.payloadId;
        observation.identity.renderAttempt = input.acknowledgement.attempt;
        result.observations.append(observation);
        return result;
    }
    const auto oldStatus = access.display().status;
    if (input.pendingTargetCommit)
        publishReady(access.request(), access.display(),
            providerFor(access.providerFacts(), ImageViewportPageRole::Primary),
            input.preparedPayload);
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
            shown.displayedPayload = shown.pendingRenderPayload;
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
    ViewportEngineRenderAcknowledgementInput input, // NOLINT(performance-unnecessary-value-param)
    ViewportEngineRenderFailureAccess access)
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
            const bool siblingPending
                = access.display().roles[1U - index].pendingRenderPayload.commitPending;
            changes.scheduleUpdate = siblingPending;
            return result;
        }
    }
    const bool pending = waitingForRender(access.request())
        && pendingSpreadReady(access.display(), access.request());
    const auto failedIdentity = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
        input.acknowledgement, input.acknowledgement.failedRole);
    const auto failedIndex
        = input.acknowledgement.failedRole == ImageViewportPageRole::Secondary ? 1U : 0U;
    const bool committedFailureMatches = input.committedDisplayAttempt
        && ViewportEngineRenderAcknowledgement::payloadMatches(
            failedIdentity, access.display().roles[failedIndex].displayedPayload.identity());
    if (terminalSealed(access.request()) || input.acknowledgement.attempt != input.attempt
        || (!committedFailureMatches
            && !ViewportEngineRenderAcknowledgement::failureMatches(
                access.display(), access.request(), input.acknowledgement))
        || (!committedFailureMatches && access.display().status != ImageViewportDisplayStatus::Ready
            && !pending)) {
        const auto failed = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
            input.acknowledgement, input.acknowledgement.failedRole);
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::Engine;
        observation.category = InternalObservationCategory::StaleDrop;
        observation.cause = InternalObservationCause::StaleRenderAcknowledgement;
        observation.identity.roleValid = true;
        observation.identity.role = input.acknowledgement.failedRole;
        observation.identity.generation = failed.generation;
        observation.identity.requestId = input.acknowledgement.targetSpread.requestId;
        observation.identity.payloadId = failed.payloadId;
        observation.identity.renderAttempt = input.acknowledgement.attempt;
        observation.detail = int(input.acknowledgement.failureCause);
        result.observations.append(observation);
        return result;
    }
    const auto oldStatus = access.display().status;
    const bool retainCommittedDisplay = access.display().hasReadyDisplay(true);
    const auto failed = ViewportEngineRenderAcknowledgement::acknowledgedPayload(
        input.acknowledgement, input.acknowledgement.failedRole);
    result.diagnostic = { true, input.acknowledgement.failedRole, failed.generation,
        input.acknowledgement.targetSpread.requestId, failed.payloadId,
        input.acknowledgement.failureCause, input.acknowledgement.attempt };
    access.request().lastAcceptedRenderFailure = result.diagnostic;
    changes.renderFailureDiagnostic = result.diagnostic;
    access.display().clearPendingRenderPayload();
    if (retainCommittedDisplay) {
        access.display().status = ImageViewportDisplayStatus::Retained;
    } else {
        access.display().status = ImageViewportDisplayStatus::Empty;
        access.display().clearDisplayedDisplay();
    }
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
            const auto admission
                = stageBuiltIn(access.request(), access.display(), input.exactnessPreference);
            if (!admission.accepted) {
                changes.requestState = true;
                changes.requestRevision = true;
                changes.displayState = true;
                changes.displayRevision = true;
                changes.diagnostics = true;
                return result;
            }
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
