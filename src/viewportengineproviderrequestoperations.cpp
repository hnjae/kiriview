#include "viewportengineproviderrequestoperations_p.h"

#include "imageviewporttoken_p.h"

#include <limits>

namespace {
using namespace ImageViewportInternal;

DisplayRequest& requestForRole(RequestState& request, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? request.roles[1].activeRequest
                                                    : request.roles[0].activeRequest;
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

bool sameSelectionDemand(
    const ImageSequenceProviderDisplayDemand& lhs, const ImageSequenceProviderDisplayDemand& rhs)
{
    return lhs.role() == rhs.role() && lhs.resolvedFrame() == rhs.resolvedFrame()
        && lhs.requestedPosition() == rhs.requestedPosition()
        && lhs.sourceLogicalSize() == rhs.sourceLogicalSize()
        && lhs.visibleSourceRect() == rhs.visibleSourceRect()
        && lhs.targetDisplaySizePixels() == rhs.targetDisplaySizePixels()
        && lhs.effectiveDevicePixelRatio() == rhs.effectiveDevicePixelRatio()
        && lhs.rotationDegrees() == rhs.rotationDegrees()
        && lhs.mirrorHorizontally() == rhs.mirrorHorizontally()
        && lhs.mirrorVertically() == rhs.mirrorVertically()
        && lhs.qualityPreference() == rhs.qualityPreference()
        && lhs.exactnessPreference() == rhs.exactnessPreference()
        && lhs.maximumTextureSize() == rhs.maximumTextureSize()
        && lhs.maximumPayloadBytes() == rhs.maximumPayloadBytes()
        && lhs.displayByteBudget() == rhs.displayByteBudget()
        && lhs.allocationGeneration() == rhs.allocationGeneration();
}

struct RequestContext
{
    RequestState& request;
    DisplayState& display;
    std::array<ViewportEngineRoleState, 2>& roles;
};

template <typename Allocate, typename Demand>
ViewportProviderFrameRequestStartResult startFrameRequest(RequestContext context,
    ImageViewportPageRole role, DisplayRequestTarget target,
    const ViewportEngineGeometryInput& geometry, Allocate allocate, Demand demand)
{
    ViewportProviderFrameRequestStartResult result;
    auto& provider = context.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U].provider;
    clearQueue(provider.requests);
    TargetSpreadWaitState wait;
    if (role == ImageViewportPageRole::Secondary) {
        wait.requiresSecondary = true;
        wait.secondary.providerWaiting = true;
    } else {
        wait.primary.providerWaiting = true;
    }
    context.request.status = ImageViewportRequestStatus::Loading;
    context.request.reason = projectWaitReason(wait);
    context.display.status = context.display.roles[0].displayedPayload.hasPresentableContent()
        ? ImageViewportDisplayStatus::Retained
        : ImageViewportDisplayStatus::Empty;

    const auto allocation = allocate(role);
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    provider.requests.activeFrameToken = allocation.token;
    provider.requests.activeFrameRefinement = false;
    if (!provider.requests.activeFrameToken.isValid()) {
        return result;
    }
    if (role == ImageViewportPageRole::Secondary) {
        const int position = provider.facts.timedMetadata
            ? provider.facts.timingIntervals.frameStartPosition(target.frame)
            : -1;
        auto& secondary = context.request.roles[1].activeRequest;
        secondary.identity = context.request.roles[0].activeRequest.identity;
        secondary.target = target;
        secondary.resolvedFrame = { target.frame, position };
        secondary.providerFrameToken = {};
        secondary.preparedPayloadId = context.request.roles[0].activeRequest.preparedPayloadId;
        if (target.providerTargetKind != ProviderRequestTargetKind::Playback && target.frame >= 0) {
            context.request.roles[1].latestNonPlaybackRequest = secondary;
        }
    }
    auto& active = requestForRole(context.request, role);
    active.providerFrameToken = provider.requests.activeFrameToken;
    result.accepted = true;
    result.sendCommand = provider.session.sessionActive;
    result.command.token = provider.requests.activeFrameToken;
    result.command.frame = active.resolvedFrame.frame;
    result.command.position = active.target.position;
    result.command.targetKind = active.target.providerTargetKind;
    result.command.demand = demand(role, geometry);
    provider.requests.lastFrameDemand = result.command.demand;
    provider.requests.hasLastFrameDemand = true;
    return result;
}

template <typename Allocate, typename Demand>
ViewportProviderFrameRequestStartResult startRefinementRequest(RequestContext context,
    ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry, Allocate allocate,
    Demand demand)
{
    ViewportProviderFrameRequestStartResult result;
    const auto index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
    auto& provider = context.roles[index].provider;
    auto& active = requestForRole(context.request, role);
    const auto allocation = allocate(role);
    result.closeSession = allocation.closeSession;
    result.sessionClose = allocation.sessionClose;
    provider.requests.activeFrameToken = allocation.token;
    provider.requests.activeFrameRefinement = allocation.token.isValid();
    if (!allocation.token.isValid())
        return result;
    active.providerFrameToken = allocation.token;
    result.accepted = true;
    result.sendCommand = provider.session.sessionActive;
    result.command.token = allocation.token;
    result.command.frame = active.resolvedFrame.frame;
    result.command.position = active.target.position;
    result.command.targetKind = ProviderRequestTargetKind::Frame;
    result.command.demand = demand(role, geometry);
    provider.requests.lastFrameDemand = result.command.demand;
    provider.requests.hasLastFrameDemand = true;
    return result;
}

}

#define DEFINE_REQUEST_ACCESS(Type)                                                                \
    ViewportProviderRequestTokenAllocationResult Type::allocate(ImageViewportPageRole role)        \
    {                                                                                              \
        return allocateViewportProviderRequestToken(                                               \
            { role }, { m_roles, m_request, m_playback, m_display });                              \
    }                                                                                              \
    ImageSequenceProviderDisplayDemand Type::demand(                                               \
        ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry)                   \
    {                                                                                              \
        if (m_nextRevision == std::numeric_limits<quint64>::max()) {                               \
            qFatal("ImageViewport revision token allocator exhausted");                            \
        }                                                                                          \
        auto& active = requestForRole(m_request, role);                                            \
        active.demandRevision                                                                      \
            = ImageViewportInternal::RevisionTokenPrivateAccess::demandFromValue(                  \
                ++m_nextRevision);                                                                 \
        const quint64 revision = m_presentationRevision != 0 ? m_presentationRevision              \
                                                             : m_presentationTargetGeneration;     \
        return projectViewportProviderDemand(                                                      \
            { role, geometry, active.demandRevision,                                               \
                ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(        \
                    m_request.requestRevision),                                                    \
                ImageViewportInternal::RevisionTokenPrivateAccess::publicRevisionFromValue(        \
                    revision),                                                                     \
                ImageViewportInternal::RevisionTokenPrivateAccess::generationFromValue(            \
                    m_presentationTargetGeneration) },                                             \
            { m_request, m_display, { m_roles[0].provider.facts, m_roles[1].provider.facts },      \
                m_presentation });                                                                 \
    }

DEFINE_REQUEST_ACCESS(ViewportEngineProviderSessionOpenedAccess)
DEFINE_REQUEST_ACCESS(ViewportEngineProviderQueueFlushAccess)
DEFINE_REQUEST_ACCESS(ViewportEngineProviderDemandRestageAccess)
DEFINE_REQUEST_ACCESS(ViewportEngineProviderFrameRequestAccess)
#undef DEFINE_REQUEST_ACCESS

ViewportProviderSessionOpenResult reduceViewportEngineProviderSessionOpened(
    ViewportEngineProviderSessionOpenedInput input,
    ViewportEngineProviderSessionOpenedAccess access)
{
    ViewportProviderSessionOpenResult result;
    const auto& terminal = access.m_request.targetSpreadTerminal;
    if (terminal.sealed && terminal.generation == access.m_request.sequenceGeneration
        && terminal.requestId == access.m_request.roles[0].activeRequest.identity.id) {
        return result;
    }
    auto& provider
        = access.m_roles[input.role == ImageViewportPageRole::Secondary ? 1U : 0U].provider;
    if (!provider.facts.metadataReady) {
        const auto allocation = access.allocate(input.role);
        provider.requests.activeMetadataToken = allocation.token;
        result.providerMetadataTransport.closeSession = allocation.closeSession;
        result.providerMetadataTransport.sessionClose = allocation.sessionClose;
        result.providerMetadataTransport.sendCommand
            = provider.session.sessionActive && allocation.token.isValid();
        result.providerMetadataTransport.token = allocation.token;
        return result;
    }
    access.m_display.clearPendingRenderPayload();
    const auto target = requestForRole(access.m_request, input.role).target;
    const auto start = startFrameRequest(
        { access.m_request, access.m_display, access.m_roles }, input.role, target, input.geometry,
        [&access](ImageViewportPageRole role) { return access.allocate(role); },
        [&access](ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) {
            return access.demand(role, geometry);
        });
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    return result;
}

ViewportProviderFrameQueueFlushResult reduceViewportEngineProviderQueueFlush(
    ViewportEngineProviderQueueFlushInput input, ViewportEngineProviderQueueFlushAccess access)
{
    ViewportProviderFrameQueueFlushResult result;
    auto& provider
        = access.m_roles[input.role == ImageViewportPageRole::Secondary ? 1U : 0U].provider;
    auto& queued = provider.requests;
    const auto& active = requestForRole(access.m_request, input.role);
    const bool providerPresent = input.role == ImageViewportPageRole::Primary
        ? access.m_request.roles[0].source.facts.provider
        : access.m_request.roles[1].sequence && access.m_request.roles[1].provider;
    const bool current = queued.queuedFrameRequest && providerPresent
        && provider.session.sessionActive
        && queued.queuedFrameGeneration == access.m_request.sequenceGeneration
        && queued.queuedFrameRequestId == active.identity.id
        && access.m_request.status == ImageViewportRequestStatus::Loading
        && access.m_request.reason == ImageViewportRequestReason::RequestQueued
        && active.target.frame == queued.queuedFrame
        && active.target.position == queued.queuedPosition
        && active.resolvedFrame.frame == queued.queuedResolvedFrame.frame
        && active.resolvedFrame.position == queued.queuedResolvedFrame.position
        && active.target.providerTargetKind == queued.queuedFrameTargetKind;
    const int frame = queued.queuedFrame;
    const auto kind = queued.queuedFrameTargetKind;
    clearQueue(queued);
    if (!current) {
        return result;
    }
    const DisplayRequestTarget target { frame, active.target.position, kind };
    const auto start = startFrameRequest(
        { access.m_request, access.m_display, access.m_roles }, input.role, target, input.geometry,
        [&access](ImageViewportPageRole role) { return access.allocate(role); },
        [&access](ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) {
            return access.demand(role, geometry);
        });
    result.providerFrameTransport.closeSession = start.closeSession;
    result.providerFrameTransport.sessionClose = start.sessionClose;
    result.providerFrameTransport.sendCommand = start.sendCommand;
    result.providerFrameTransport.command = start.command;
    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.diagnostics = access.m_request.status == ImageViewportRequestStatus::Error
        && access.m_request.reason == ImageViewportRequestReason::ProviderFailure;
    return result;
}

std::array<ViewportProviderFrameTransportEffect, 2> reduceViewportEngineProviderDemandRestage(
    ViewportEngineProviderDemandRestageInput input,
    ViewportEngineProviderDemandRestageAccess access)
{
    std::array<ViewportProviderFrameTransportEffect, 2> effects;
    for (const auto role : { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        const std::size_t index = role == ImageViewportPageRole::Secondary ? 1U : 0U;
        auto& provider = access.m_roles[index].provider;
        auto& request = requestForRole(access.m_request, role);
        const bool present = role == ImageViewportPageRole::Primary
            ? access.m_request.roles[0].source.facts.provider
            : access.m_request.roles[1].sequence && access.m_request.roles[1].provider;
        if (!present || !provider.session.sessionActive || !provider.facts.metadataReady
            || request.identity.id == 0 || request.resolvedFrame.frame < 0) {
            continue;
        }
        const auto previousRevision = request.demandRevision;
        const auto projected = access.demand(role, input.geometry);
        if (provider.requests.hasLastFrameDemand
            && sameSelectionDemand(projected, provider.requests.lastFrameDemand)) {
            request.demandRevision = previousRevision;
            continue;
        }
        if (!provider.requests.activeFrameToken.isValid()
            && access.m_request.status != ImageViewportRequestStatus::Ready) {
            request.demandRevision = previousRevision;
            continue;
        }
        const bool ordinaryRequestActive = provider.requests.activeFrameToken.isValid()
            && !provider.requests.activeFrameRefinement;
        effects[index].cancelToken = provider.requests.activeFrameToken;
        provider.requests.activeFrameToken = {};
        provider.requests.activeFrameRefinement = false;
        request.providerFrameToken = {};
        access.m_display.roles[index].pendingRenderPayload = {};
        const bool committedTarget = access.m_request.status == ImageViewportRequestStatus::Ready
            && access.m_display.status == ImageViewportDisplayStatus::Ready
            && access.m_display.roles[index].displayedRequest.generation
                == access.m_request.sequenceGeneration
            && access.m_display.roles[index].displayedRequest.request.resolvedFrame.frame
                == request.resolvedFrame.frame
            && access.m_display.roles[index].displayedRequest.request.resolvedFrame.position
                == request.resolvedFrame.position;
        const auto start = ordinaryRequestActive || !committedTarget
            ? startFrameRequest(
                  { access.m_request, access.m_display, access.m_roles }, role, request.target,
                  input.geometry,
                  [&access](ImageViewportPageRole selected) { return access.allocate(selected); },
                  [&access](
                      ImageViewportPageRole selected, const ViewportEngineGeometryInput& geometry) {
                      return access.demand(selected, geometry);
                  })
            : startRefinementRequest(
                  { access.m_request, access.m_display, access.m_roles }, role, input.geometry,
                  [&access](ImageViewportPageRole selected) { return access.allocate(selected); },
                  [&access](
                      ImageViewportPageRole selected, const ViewportEngineGeometryInput& geometry) {
                      return access.demand(selected, geometry);
                  });
        effects[index].closeSession = start.closeSession;
        effects[index].sessionClose = start.sessionClose;
        effects[index].sendCommand = start.sendCommand;
        effects[index].command = start.command;
    }
    return effects;
}

ViewportProviderFrameRequestStartResult startViewportEngineProviderFrameRequest(
    ViewportEngineProviderFrameRequestInput input, ViewportEngineProviderFrameRequestAccess access)
{
    return startFrameRequest(
        { access.m_request, access.m_display, access.m_roles }, input.role, input.target,
        input.geometry, [&access](ImageViewportPageRole role) { return access.allocate(role); },
        [&access](ImageViewportPageRole role, const ViewportEngineGeometryInput& geometry) {
            return access.demand(role, geometry);
        });
}
