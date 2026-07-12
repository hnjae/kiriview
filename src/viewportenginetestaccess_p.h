#pragma once

#include "viewportengine_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportenginestate_p.h"

#include <utility>

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
class ViewportEngineTestAccess
{
public:
    static ViewportEngineCommandDiagnostics commandDiagnostics(const ViewportEngine& engine)
    {
        return engine.commandDiagnostics();
    }
    static ViewportEnginePresentationTargetState presentationTargetState(
        const ViewportEngine& engine)
    {
        return engine.presentationTargetState();
    }
    static const ImageViewportInternal::PresentationState& presentation(
        const ViewportEngine& engine)
    {
        return engine.presentationState();
    }
    static ViewportRenderSnapshot renderSnapshot(
        const ViewportEngine& engine, const ViewportRenderSnapshotInput& input)
    {
        return engine.renderSnapshot(input);
    }
    static ImageViewportInternal::DisplayState& display(ViewportEngine& engine)
    {
        return engine.displayState();
    }
    static const ImageViewportInternal::DisplayState& display(const ViewportEngine& engine)
    {
        return engine.displayState();
    }
    static ImageViewportInternal::RequestState& request(ViewportEngine& engine)
    {
        return engine.requestState();
    }
    static const ImageViewportInternal::RequestState& request(const ViewportEngine& engine)
    {
        return engine.requestState();
    }
    static ImageViewportInternal::PlaybackState& playback(ViewportEngine& engine)
    {
        return engine.playbackState();
    }
    static const ImageViewportInternal::PlaybackState& playback(const ViewportEngine& engine)
    {
        return engine.playbackState();
    }
    static ViewportPlaybackScheduleEffect playbackSchedule(const ViewportEngine& engine)
    {
        return engine.currentPlaybackSchedule();
    }
    static ViewportEngineAuthoredAutoplayReduction reduceAuthoredAutoplay(ViewportEngine& engine)
    {
        ViewportEngineAuthoredAutoplayAccess access(
            engine.m_state->requestState.request.roles[0].source,
            engine.m_state->providerState.roles[0].provider.facts,
            engine.m_state->requestState.request.roles[0].activeRequest,
            engine.m_state->playbackState.playback, engine.m_state->requestState.request.status);
        return reduceViewportEngineAuthoredAutoplay({}, std::move(access));
    }
    static ImageViewport::CommandReason& commandReason(ViewportEngine& engine)
    {
        return engine.m_state->commandState.reason;
    }
    static quint64& publishedCommandRevision(ViewportEngine& engine)
    {
        return engine.m_state->commandState.publishedRevision;
    }
    static quint64 publishedCommandRevision(const ViewportEngine& engine)
    {
        return engine.m_state->commandState.publishedRevision;
    }
    static ImageViewportInternal::ProviderSessionState& providerSession(
        ViewportEngine& engine, ImageViewport::PageRole role)
    {
        return engine.m_state->providerState
            .roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U]
            .provider.session;
    }
    static void activateProviderSession(ViewportEngine& engine, ImageViewport::PageRole role)
    {
        auto& session = providerSession(engine, role);
        session.sessionActive = true;
        ++session.sessionSerial;
    }
    static ImageViewportInternal::ProviderRequestState& providerRequests(
        ViewportEngine& engine, ImageViewport::PageRole role)
    {
        return engine.m_state->providerState
            .roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U]
            .provider.requests;
    }
    static ImageViewportInternal::ProviderFactsState& providerFacts(
        ViewportEngine& engine, ImageViewport::PageRole role)
    {
        return engine.m_state->providerState
            .roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U]
            .provider.facts;
    }
};
#endif
