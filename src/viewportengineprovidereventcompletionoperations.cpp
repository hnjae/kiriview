#include "viewportengineprovidereventcompletionoperations_p.h"
#include "viewportengineprovidersessionoperations_p.h"
#include <cmath>

namespace {
using namespace ImageViewportInternal;
std::size_t index(ImageViewportPageRole r)
{
    return r == ImageViewportPageRole::Secondary ? 1U : 0U;
}
DisplayRequest& requestFor(RequestState& r, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? r.roles[1].activeRequest
                                                    : r.roles[0].activeRequest;
}
const DisplayRequest& requestFor(const RequestState& r, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Secondary ? r.roles[1].activeRequest
                                                    : r.roles[0].activeRequest;
}
bool present(const RequestState& r, ImageViewportPageRole role)
{
    return role == ImageViewportPageRole::Primary ? r.roles[0].source.facts.provider
                                                  : r.roles[1].sequence && r.roles[1].provider;
}
bool sealed(const RequestState& r)
{
    return r.targetSpreadTerminal.sealed
        && r.targetSpreadTerminal.generation == r.sequenceGeneration
        && r.targetSpreadTerminal.requestId == r.roles[0].activeRequest.identity.id;
}
void clearQueue(ProviderRequestState& q)
{
    q.queuedFrameRequest = false;
    q.queuedFrameGeneration = 0;
    q.queuedFrameRequestId = 0;
    q.queuedFrame = -1;
    q.queuedPosition = -1;
    q.queuedResolvedFrame = {};
    q.queuedFrameFromPlayback = false;
    q.queuedFrameTargetKind = ProviderRequestTargetKind::Unknown;
}
void phase(PlaybackState& p, ImageViewportPlaybackPhase v, ViewportChangeSet& c)
{
    if (p.phase != v) {
        p.phase = v;
        c.playbackPhase = true;
    }
}
bool loops(const PlaybackState& p, ImageSequenceAuthoredAnimationFacts a)
{
    if (p.looping)
        return true;
    switch (a.loopMode()) {
    case ImageSequenceAuthoredAnimationLoopMode::Unavailable:
        return false;
    case ImageSequenceAuthoredAnimationLoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationLoopMode::Finite:
        return p.loopIterationsCompleted + 1 < a.loopCount();
    case ImageSequenceAuthoredAnimationLoopMode::PlayOnce:
        return false;
    }
    return false;
}
}

ViewportEngineProviderWaitingReduction reduceViewportEngineProviderWaiting(
    ViewportEngineProviderWaitingInput in, ViewportEngineProviderWaitingAccess a)
{
    ViewportEngineProviderWaitingReduction out;
    if (sealed(a.m_request) || !present(a.m_request, in.role) || !a.m_session.sessionActive
        || (in.progress
            && (!std::isfinite(in.progressValue) || in.progressValue < 0 || in.progressValue > 1)))
        return out;
    bool mt = !a.m_facts.metadataReady && a.m_requests.activeMetadataToken.isValid()
        && in.token == a.m_requests.activeMetadataToken;
    const auto& r = requestFor(a.m_request, in.role);
    bool ft = a.m_requests.activeFrameToken.isValid() && in.token == a.m_requests.activeFrameToken
        && in.token == r.providerFrameToken;
    if (ft && a.m_requests.activeFrameRefinement)
        return out;
    if ((!mt && !ft) || a.m_request.status != ImageViewportRequestStatus::Loading
        || a.m_request.reason == ImageViewportRequestReason::ProviderWaiting)
        return out;
    a.m_request.reason = ImageViewportRequestReason::ProviderWaiting;
    out.changes.requestState = true;
    out.changes.requestRevision = true;
    return out;
}

ViewportProviderFrameRequestStartResult ViewportEngineProviderEndOfSequenceAccess::startFrame(
    ImageViewportPageRole role, ImageViewportInternal::DisplayRequestTarget target,
    const ViewportEngineGeometryInput& g)
{
    ViewportEngineProviderFrameRequestAccess a(m_request, m_playback, m_display, m_roles,
        m_presentation, m_nextRevision, m_presentationRevision, m_targetGeneration);
    return startViewportEngineProviderFrameRequest({ role, target, g }, std::move(a));
}
ViewportProviderFrameTransportEffect ViewportEngineProviderEndOfSequenceAccess::closeSession(
    ImageViewportPageRole role)
{
    auto& p = m_roles[index(role)].provider;
    ViewportEngineProviderSessionCloseAccess a(p.session, p.requests);
    return closeViewportEngineProviderSession(std::move(a));
}
ImageViewportInternal::ViewportChangeSet ViewportEngineProviderEndOfSequenceAccess::recordTerminal(
    ViewportEngineProviderTerminalProjectionInput in)
{
    ViewportEngineProviderTerminalProjectionAccess a(m_request);
    return reduceViewportEngineProviderTerminalProjection(std::move(in), std::move(a));
}

ViewportEngineProviderEndOfSequenceReduction reduceViewportEngineProviderEndOfSequence(
    ViewportEngineProviderEndOfSequenceInput in, ViewportEngineProviderEndOfSequenceAccess a)
{
    using namespace ImageViewportInternal;
    ViewportEngineProviderEndOfSequenceReduction out;
    auto& p = a.m_roles[index(in.role)].provider;
    if (!present(a.m_request, in.role) || !p.session.sessionActive)
        return out;
    bool mt = !p.facts.metadataReady && p.requests.activeMetadataToken.isValid()
        && in.token == p.requests.activeMetadataToken;
    auto& r = requestFor(a.m_request, in.role);
    bool ft = p.requests.activeFrameToken.isValid() && in.token == p.requests.activeFrameToken
        && in.token == r.providerFrameToken;
    if ((!mt && !ft) || sealed(a.m_request))
        return out;
    if (mt || !p.facts.metadataReady || !p.facts.timedMetadata
        || r.target.providerTargetKind != ProviderRequestTargetKind::Playback) {
        clearQueue(p.requests);
        if (mt)
            p.requests.activeMetadataToken = {};
        if (ft)
            p.requests.activeFrameToken = {};
        p.requests.activeFrameRefinement = false;
        a.m_playback.providerStartPending = false;
        a.m_playback.stopWhenRequestReady = false;
        out.changes = a.recordTerminal({ in.role, ImageViewportRequestStatus::Error,
            ImageViewportRequestReason::PayloadRejection,
            mt ? FailureScope::Generation : FailureScope::DisplayRequest,
            QStringLiteral("provider protocol violation"), out.changes });
        phase(a.m_playback, ImageViewportPlaybackPhase::Stopped, out.changes);
        out.providerFrameTransport = a.closeSession(in.role);
        return out;
    }
    p.requests.activeFrameToken = {};
    p.requests.activeFrameRefinement = false;
    bool dc = a.m_request.clearDiagnostics();
    bool loop = loops(a.m_playback, p.facts.authoredAnimationFacts);
    int frame = loop ? 0 : p.facts.timingIntervals.frameCount() - 1;
    int pos = loop ? 0 : p.facts.timingIntervals.frameStartPosition(frame);
    a.m_playback.position = loop ? 0 : p.facts.timingIntervals.totalDuration();
    a.m_playback.stopWhenRequestReady = !loop;
    DisplayRequestTarget target { frame, pos, ProviderRequestTargetKind::Playback };
    if (in.role == ImageViewportPageRole::Secondary) {
        auto primary = a.m_request.roles[0].activeRequest;
        a.m_request.beginDisplayRequest(
            DisplayRequestOrigin::Playback, primary.target, primary.resolvedFrame, false);
        auto& s = a.m_request.roles[1].activeRequest;
        s.identity = a.m_request.roles[0].activeRequest.identity;
        s.target = target;
        s.resolvedFrame = { frame, pos };
        s.providerFrameToken = {};
        s.preparedPayloadId = a.m_request.roles[0].activeRequest.preparedPayloadId;
    } else {
        a.m_request.roles[0].activeRequest.target = target;
        a.m_request.roles[0].activeRequest.resolvedFrame = { frame, pos };
    }
    bool same = in.role == ImageViewportPageRole::Primary && !loop
        && a.m_display.hasReadyDisplay(a.m_request.roles[0].source.facts.present)
        && a.m_display.roles[0].displayedRequest.generation == a.m_request.sequenceGeneration
        && a.m_display.roles[0].displayedRequest.request.resolvedFrame.frame == frame
        && a.m_display.roles[0].displayedRequest.request.resolvedFrame.position == pos;
    if (same) {
        a.m_request.status = ImageViewportRequestStatus::Ready;
        a.m_request.reason = ImageViewportRequestReason::Ready;
        a.m_display.status = ImageViewportDisplayStatus::Ready;
        phase(a.m_playback, ImageViewportPlaybackPhase::Stopped, out.changes);
        a.m_playback.stopWhenRequestReady = false;
        out.changes.requestState = out.changes.requestRevision = out.changes.displayState
            = out.changes.displayRevision = true;
        out.changes.diagnostics = dc;
        out.changes.scheduleUpdate = true;
        return out;
    }
    a.m_request.targetSpreadTerminal.clear();
    a.m_display.clearPendingRenderPayload();
    auto start = a.startFrame(in.role, target, in.geometry);
    out.providerFrameTransport.closeSession = start.closeSession;
    out.providerFrameTransport.sessionClose = start.sessionClose;
    out.providerFrameTransport.sendCommand = start.sendCommand;
    out.providerFrameTransport.command = start.command;
    if (loop && !a.m_playback.looping)
        ++a.m_playback.loopIterationsCompleted;
    phase(a.m_playback, ImageViewportPlaybackPhase::Waiting, out.changes);
    out.changes.requestState = out.changes.requestRevision = out.changes.displayState
        = out.changes.displayRevision = true;
    out.changes.diagnostics = dc || !start.accepted;
    out.changes.scheduleUpdate = true;
    return out;
}
