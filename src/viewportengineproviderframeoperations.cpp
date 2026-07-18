#include "viewportengineproviderframeoperations_p.h"

#include "imageviewporttoken_p.h"
#include "presentationgeometry_p.h"
#include "viewportenginebuiltinframeoperations_p.h"
#include "viewportengineprojection_p.h"
#include "viewportenginetargetspreadoperations_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"

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

bool activeTokenMatches(const ProviderRoleState& provider, const RequestState& request,
    ImageViewportPageRole role, ImageSequenceProviderRequestToken token)
{
    const auto& active = requestForRole(request, role);
    const auto* record = provider.requests.find(token);
    return record && record->isFrameWork() && record->role == role
        && record->generation == request.sequenceGeneration
        && record->requestId == active.identity.id;
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
    const ProviderRequestRecord& providerRequest, const PresentationState& presentation,
    ImageViewportPageRole role, bool refinement)
{
    const auto& active = requestForRole(request, role);
    const auto index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
    PreparedPayload preparedPayload = display.roles[index].pendingRenderPayload;
    if (!preparedPayload.identity().isValid()) {
        preparedPayload.generation = request.sequenceGeneration;
        preparedPayload.payloadId = active.identity.id == 0 ? 0 : display.nextPreparedPayloadId + 1;
    }
    const auto* demand = providerRequest.demand ? &*providerRequest.demand : nullptr;
    return { provider.facts.metadataReady, provider.facts.timedMetadata, provider.facts.logicalSize,
        provider.facts.timingIntervals, providerRequest.resolvedFrame, role, preparedPayload,
        demand ? demand->demandRevision() : ImageViewportDemandRevisionToken {},
        demand ? demand->maximumTextureSize() : -1, demand ? demand->maximumPayloadBytes() : -1,
        demand ? demand->displayByteBudget() : -1, presentation.exactnessPreference };
}

ImageViewportDisplayStatus retainedDisplayStatus(const DisplayState& display)
{
    const bool retained = (display.status == ImageViewportDisplayStatus::Ready
                              || display.status == ImageViewportDisplayStatus::Retained)
        && display.roles[0].displayedPayload.hasPresentableContent();
    return retained ? ImageViewportDisplayStatus::Retained : ImageViewportDisplayStatus::Empty;
}

void updatePlaybackPhase(
    PlaybackState& playback, ImageViewportPlaybackPhase phase, ViewportChangeSet& changes)
{
    if (playback.phase == phase) {
        return;
    }
    playback.phase = phase;
    changes.playbackPhase = true;
}

ViewportEngineBuiltInFrameStageResult stageBuiltInSecondaryPayload(RequestState& request,
    DisplayState& display, PlaybackState& playback,
    ImageViewportExactnessPreference exactnessPreference)
{
    return stageViewportEngineBuiltInTargetSpread(request, display, exactnessPreference, &playback);
}
}

ImageViewportInternal::ViewportChangeSet ViewportEngineProviderFrameReadyAccess::recordTerminal(
    ViewportEngineProviderTerminalProjectionInput input)
{
    ViewportEngineProviderTerminalProjectionAccess access(m_request);
    return reduceViewportEngineProviderDisplayRequestTerminalProjection(
        std::move(input), std::move(access));
}

ViewportEngineProviderFrameReadyReduction reduceViewportEngineProviderFrameReady(
    ViewportEngineProviderFrameReadyInput input, ViewportEngineProviderFrameReadyAccess access)
{
    using namespace ImageViewportInternal;
    ViewportEngineProviderFrameReadyReduction result;
    const bool terminalContinuation = viewportEngineHasCurrentTerminal(access.m_request);
    if (!viewportEngineRoleCanRefineCurrentTerminal(access.m_request, input.role)
        || !providerPresent(access.m_request, input.role)
        || !access.m_provider.session.sessionActive
        || !activeTokenMatches(access.m_provider, access.m_request, input.role, input.token)) {
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::Engine;
        observation.category = InternalObservationCategory::StaleDrop;
        observation.cause = InternalObservationCause::ProviderTokenMismatch;
        observation.identity.roleValid = true;
        observation.identity.role = input.role;
        observation.identity.generation = access.m_request.sequenceGeneration;
        observation.identity.requestId = requestForRole(access.m_request, input.role).identity.id;
        observation.identity.providerToken = ProviderRequestTokenPrivateAccess::value(input.token);
        observation.identity.demandRevision = DemandRevisionTokenPrivateAccess::value(
            requestForRole(access.m_request, input.role).demandRevision);
        observation.identity.providerLeaseId = input.providerFrameLeaseId;
        result.observations.append(observation);
        return result;
    }

    const auto* activeProviderRequest = access.m_provider.requests.find(input.token);
    if (!activeProviderRequest) {
        return result;
    }
    const ProviderRequestRecord providerRequest = *activeProviderRequest;
    const bool refinement = providerRequest.isRefinement();

    if (input.role == ImageViewportPageRole::Secondary && !refinement && !terminalContinuation) {
        auto& preparedPayload = access.m_display.roles[0].pendingRenderPayload;
        auto& primaryRequest = access.m_request.roles[0].activeRequest;
        if (!preparedPayload.identity().isValid()) {
            if (displayedPrimaryPayloadMatchesActiveTarget(access.m_display, access.m_request)) {
                preparedPayload = access.m_display.roles[0].displayedPayload;
            } else {
                preparedPayload.generation = access.m_request.sequenceGeneration;
                preparedPayload.payloadId = ++access.m_display.nextPreparedPayloadId;
            }
            preparedPayload.commitPending = true;
            primaryRequest.preparedPayloadId = preparedPayload.payloadId;
        }
        access.m_provider.requests.retire(input.token);
    }

    const auto frameState = preparationState(access.m_request, access.m_display, access.m_provider,
        providerRequest, access.m_presentation, input.role, refinement);
    const auto admission
        = FramePreparation::admitProviderFrame(input.frame, input.envelope, frameState);
    if (!admission.accepted()) {
        InternalObservation observation;
        observation.subsystem = InternalObservationSubsystem::Preparation;
        observation.category = InternalObservationCategory::AdmissionFailure;
        observation.cause = InternalObservationCause::ProviderFrameRejected;
        observation.identity.roleValid = true;
        observation.identity.role = input.role;
        observation.identity.generation = access.m_request.sequenceGeneration;
        observation.identity.requestId = requestForRole(access.m_request, input.role).identity.id;
        observation.identity.providerToken = ProviderRequestTokenPrivateAccess::value(input.token);
        observation.identity.demandRevision = DemandRevisionTokenPrivateAccess::value(
            requestForRole(access.m_request, input.role).demandRevision);
        observation.identity.providerLeaseId = input.providerFrameLeaseId;
        observation.detail = int(admission.cause);
        result.observations.append(observation);
        access.m_provider.requests.clearQueue();
        access.m_provider.requests.retire(input.token);
        if (refinement)
            return result;
        result.changes = access.recordTerminal({ input.role, admission.status, admission.reason,
            PublicDiagnosticText::fromUntrusted(admission.diagnostic), result.changes });
        updatePlaybackPhase(access.m_playback, ImageViewportPlaybackPhase::Stopped, result.changes);
        return result;
    }

    auto admittedPayload = admission.preparedPayload;
    admittedPayload.providerFrameLeaseId = input.providerFrameLeaseId;

    const auto oldRequestStatus = access.m_request.status;
    const auto oldRequestReason = access.m_request.reason;
    const auto oldGeometry = projectViewportGeometryState(input.geometry, access.m_presentation);
    access.m_provider.requests.retire(input.token);

    if (terminalContinuation && !refinement) {
        result.changes = projectViewportEngineCurrentTerminal(result.changes, access.m_request);
        return result;
    }

    if (refinement) {
        const auto index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
        admittedPayload.commitPending = true;
        access.m_display.roles[index].pendingRenderPayload = admittedPayload;
        requestForRole(access.m_request, input.role).preparedPayloadId = admittedPayload.payloadId;
        result.changes.scheduleUpdate = true;
        return result;
    }

    coalesceViewportEngineTargetSpreadCandidates(access.m_request, access.m_display);
    const bool diagnosticsChanged = access.m_request.clearError();

    if (input.role == ImageViewportPageRole::Secondary) {
        admittedPayload.commitPending = true;
        access.m_display.roles[1].pendingRenderPayload = admittedPayload;
        access.m_request.roles[1].activeRequest.preparedPayloadId = admittedPayload.payloadId;
        if (admittedPayload.payloadId > access.m_display.nextPreparedPayloadId) {
            access.m_display.nextPreparedPayloadId = admittedPayload.payloadId;
        }
        const bool primaryReady = access.m_display.roles[0].pendingRenderPayload.commitPending
            && !access.m_display.roles[0].pendingRenderPayload.image.isNull();
        TargetSpreadWaitState wait;
        wait.requiresSecondary = true;
        if (primaryReady
            && (!input.geometry.renderAvailable || input.geometry.itemBounds.isEmpty())) {
            wait.primary.renderWaiting = true;
            wait.secondary.renderWaiting = true;
        } else if (primaryReady) {
            wait.primary.uploadPending = true;
            wait.secondary.uploadPending = true;
        } else {
            wait.primary.providerWaiting = true;
            wait.secondary.uploadPending = true;
        }
        access.m_request.status = ImageViewportRequestStatus::Loading;
        access.m_request.reason = projectWaitReason(wait);
        access.m_display.status = retainedDisplayStatus(access.m_display);
    } else {
        access.m_request.targetSpreadTerminal.clear();
        access.m_display.commitPreparedPayloadIdentity(
            access.m_request.roles[0].activeRequest, admittedPayload);
        const auto builtInAdmission = stageBuiltInSecondaryPayload(access.m_request,
            access.m_display, access.m_playback, access.m_presentation.exactnessPreference);
        if (!builtInAdmission.accepted) {
            result.changes.requestState = true;
            result.changes.requestRevision = true;
            result.changes.displayState = true;
            result.changes.displayRevision = true;
            result.changes.diagnostics = true;
            result.changes.playbackPhase = builtInAdmission.playbackStopped;
            return result;
        }
        TargetSpreadWaitState wait;
        wait.requiresSecondary = access.m_request.roles[1].sequence
            && (access.m_request.roles[1].provider
                || access.m_request.roles[1].activeRequest.target.frame >= 0);
        if (!input.geometry.renderAvailable || input.geometry.itemBounds.isEmpty()) {
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
        access.m_request.status = ImageViewportRequestStatus::Loading;
        access.m_request.reason = projectWaitReason(wait);
        access.m_display.status = retainedDisplayStatus(access.m_display);
        access.m_display.roles[0].pendingRenderPayload.commitPending = true;
        if (access.m_playback.phase == ImageViewportPlaybackPhase::Waiting
            && access.m_request.status == ImageViewportRequestStatus::Ready
            && !access.m_display.roles[0].pendingRenderPayload.commitPending) {
            updatePlaybackPhase(access.m_playback,
                access.m_playback.stopWhenRequestReady ? ImageViewportPlaybackPhase::Stopped
                                                       : ImageViewportPlaybackPhase::Playing,
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
