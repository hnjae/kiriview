#include "viewportenginetargetspreadoperations_p.h"

#include "imageviewporttoken_p.h"

#include <limits>

namespace {
using namespace ImageViewportInternal;

DisplayRequest& requestForRole(RequestState& request, ImageViewportPageRole role)
{
    return request.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].activeRequest;
}

void mergeChanges(ViewportChangeSet& target, const ViewportChangeSet& source)
{
    target.requestState |= source.requestState;
    target.displayState |= source.displayState;
    target.playbackPhase |= source.playbackPhase;
    target.diagnostics |= source.diagnostics;
    target.displayRevision |= source.displayRevision;
    target.requestRevision |= source.requestRevision;
    target.presentationRevision |= source.presentationRevision;
    target.targetPresentationRevision |= source.targetPresentationRevision;
    target.scheduleUpdate |= source.scheduleUpdate;
}

bool matchesActiveTarget(const ImageViewportInternal::RequestState& request,
    const ImageViewportInternal::DisplayState::RoleState& displayRole, std::size_t index)
{
    const auto& requestRole = request.roles[index];
    return requestRole.source.facts.present && displayRole.displayedPayload.hasPresentableContent()
        && displayRole.displayedRequest.generation == request.sequenceGeneration
        && displayRole.displayedRequest.request.resolvedFrame.frame
        == requestRole.activeRequest.resolvedFrame.frame
        && displayRole.displayedRequest.request.resolvedFrame.position
        == requestRole.activeRequest.resolvedFrame.position;
}
}

void invalidateViewportEngineTargetSpreadRole(ImageViewportInternal::RequestState& request,
    ImageViewportInternal::DisplayState& display, ImageViewportPageRole role)
{
    const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
    display.roles[index].pendingRenderPayload = {};
    request.roles[index].activeRequest.preparedPayloadId = 0;
}

void coalesceViewportEngineTargetSpreadCandidates(
    ImageViewportInternal::RequestState& request, ImageViewportInternal::DisplayState& display)
{
    for (std::size_t index = 0; index < request.roles.size(); ++index) {
        auto& pending = display.roles[index].pendingRenderPayload;
        auto& active = request.roles[index].activeRequest;
        if (pending.hasPresentableContent() && pending.generation == request.sequenceGeneration
            && pending.resolvedFrame.frame == active.resolvedFrame.frame
            && pending.resolvedFrame.position == active.resolvedFrame.position) {
            pending.commitPending = true;
            active.preparedPayloadId = pending.payloadId;
            continue;
        }
        if (!matchesActiveTarget(request, display.roles[index], index)) {
            continue;
        }
        pending = display.roles[index].displayedPayload;
        pending.commitPending = true;
        active.preparedPayloadId = pending.payloadId;
    }
}

ViewportEngineProviderRoleMaterializationResult materializeViewportEngineProviderRole(
    ViewportEngineProviderRoleMaterializationInput input,
    ViewportEngineProviderRoleMaterializationAccess& access)
{
    using namespace ImageViewportInternal;
    ViewportEngineProviderRoleMaterializationResult result;
    const std::size_t index = input.role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& provider = access.roles[index].provider;
    auto& active = requestForRole(access.request, input.role);
    auto& effect = result.effect;

    invalidateViewportEngineTargetSpreadRole(access.request, access.display, input.role);
    const auto* frameRequest = provider.requests.frameRequest();
    if (frameRequest && frameRequest->isRefinement()) {
        effect.cancelToken = frameRequest->token;
        provider.requests.retire(frameRequest->token);
        frameRequest = nullptr;
    }
    if (frameRequest) {
        TargetSpreadWaitState wait;
        if (input.role == ImageViewportPageRole::Secondary) {
            wait.requiresSecondary = true;
            wait.secondary.requestQueued = true;
        } else {
            wait.primary.requestQueued = true;
        }
        access.request.status = ImageViewportRequestStatus::Loading;
        access.request.reason = projectWaitReason(wait);
        const bool retained = (access.display.status == ImageViewportDisplayStatus::Ready
                                  || access.display.status == ImageViewportDisplayStatus::Retained)
            && access.display.roles[0].displayedPayload.hasPresentableContent();
        access.display.status
            = retained ? ImageViewportDisplayStatus::Retained : ImageViewportDisplayStatus::Empty;
        if (provider.session.sessionActive) {
            effect.cancelToken = frameRequest->token;
        }
        provider.requests.retire(frameRequest->token);
        provider.requests.queue({ access.request.sequenceGeneration, active.identity.id,
            active.target, active.resolvedFrame, input.fromPlayback });
        effect.deferredEngineEvent = ViewportProviderDeferredEngineEvent::FlushQueuedFrameRequest;
        result.accepted = true;
        return result;
    }

    ViewportProviderRequestTokenAllocationAccess allocationAccess(
        access.roles, access.request, access.playback, access.display);
    auto allocation = allocateViewportProviderRequestToken({ input.role }, allocationAccess);
    auto allocationMutation = allocationAccess.takeMutation();
    access.roles = std::move(allocationMutation.roles);
    access.request = std::move(allocationMutation.request);
    access.playback = std::move(allocationMutation.playback);
    access.display = std::move(allocationMutation.display);
    effect.closeSession = allocation.closeSession;
    effect.sessionClose = allocation.sessionClose;
    mergeChanges(result.changes, allocation.changes);
    if (allocation.exhausted) {
        return result;
    }

    effect.sendCommand = provider.session.sessionActive;
    effect.command.token = allocation.token;
    effect.command.frame = active.resolvedFrame.frame;
    effect.command.position = active.target.position;
    effect.command.targetKind = active.target.providerTargetKind;

    if (access.nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    active.demandRevision = DemandRevisionTokenPrivateAccess::fromValue(++access.nextRevision);
    const quint64 presentationRevision = access.presentationRevision != 0
        ? access.presentationRevision
        : access.presentationTargetGeneration;
    effect.command.demand = projectViewportProviderDemand(
        { input.role, input.geometry, active.demandRevision,
            RevisionTokenPrivateAccess::publicRevisionFromValue(access.request.requestRevision),
            RevisionTokenPrivateAccess::publicRevisionFromValue(presentationRevision),
            AllocationGenerationTokenPrivateAccess::fromValue(
                access.presentationTargetGeneration) },
        { access.request, access.display,
            { access.roles[0].provider.facts, access.roles[1].provider.facts },
            access.presentation });
    provider.requests.activate(
        { allocation.token, providerRequestKind(active.target.providerTargetKind), input.role,
            access.request.sequenceGeneration, active.identity.id,
            ProviderRequestOwnership::DisplayRequest, active.target, active.resolvedFrame,
            effect.command.demand });
    provider.requests.lastIssuedFrameDemand = effect.command.demand;
    TargetSpreadWaitState wait;
    if (input.role == ImageViewportPageRole::Secondary) {
        wait.requiresSecondary = true;
        wait.secondary.providerWaiting = true;
    } else {
        wait.primary.providerWaiting = true;
    }
    access.request.status = ImageViewportRequestStatus::Loading;
    access.request.reason = projectWaitReason(wait);
    access.display.status = access.display.roles[0].displayedPayload.hasPresentableContent()
        ? ImageViewportDisplayStatus::Retained
        : ImageViewportDisplayStatus::Empty;
    result.accepted = true;
    return result;
}

ViewportEngineBuiltInFrameStageResult materializeViewportEngineBuiltInTargetSpread(
    ImageViewportInternal::RequestState& request, ImageViewportInternal::PlaybackState& playback,
    ImageViewportInternal::DisplayState& display,
    const ImageViewportInternal::PresentationState& presentation,
    const ViewportEngineGeometryInput& geometry)
{
    const auto admission = stageViewportEngineBuiltInTargetSpread(
        request, display, presentation.exactnessPreference, &playback);
    if (!admission.accepted) {
        return admission;
    }
    request.status = ImageViewportRequestStatus::Loading;
    request.reason = (!geometry.renderAvailable || geometry.itemBounds.isEmpty())
        ? ImageViewportRequestReason::RenderWaiting
        : ImageViewportRequestReason::UploadPending;
    display.status = display.roles[0].displayedPayload.hasPresentableContent()
        ? ImageViewportDisplayStatus::Retained
        : ImageViewportDisplayStatus::Empty;
    return admission;
}
