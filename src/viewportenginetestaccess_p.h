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
    static PresentationGeometry::State geometryState(const ViewportEngine& engine)
    {
        return engine.geometryState();
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
        ViewportEngineAuthoredAutoplayAccess access(engine.m_state->requestState.request,
            { engine.m_state->providerState.roles[0].provider.facts,
                engine.m_state->providerState.roles[1].provider.facts },
            engine.m_state->playbackState.playback);
        auto reduction = reduceViewportEngineAuthoredAutoplay({}, access);
        engine.m_state->playbackState.playback = std::move(access.takeMutation().playback);
        return reduction;
    }
    static ImageViewportCommandReason& commandReason(ViewportEngine& engine)
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
    static void setNextRevisionValue(ViewportEngine& engine, quint64 value)
    {
        engine.setNextRevisionValueForTest(value);
    }
    static quint64 currentRenderAttempt(const ViewportEngine& engine)
    {
        return engine.m_state->renderCoordination.nextSynchronizationAttempt;
    }
    static void restoreViewportStatePreservingActiveRenderAttempt(
        ViewportEngine& engine, ViewportEngineViewportState viewport)
    {
        const auto activeAttempt = engine.m_state->renderCoordination.activeAttempt;
        engine.handleViewportChanged(viewport);
        engine.m_state->renderCoordination.activeAttempt = activeAttempt;
    }
    static ImageViewportInternal::ProviderSessionState& providerSession(
        ViewportEngine& engine, ImageViewportPageRole role)
    {
        return engine.m_state->providerState
            .roles[role == ImageViewportPageRole::Secondary ? 1U : 0U]
            .provider.session;
    }
    static void activateProviderSession(ViewportEngine& engine, ImageViewportPageRole role)
    {
        auto& session = providerSession(engine, role);
        session.sessionActive = true;
        ++session.sessionSerial;
    }
    static ImageViewportInternal::ProviderRequestLedger& providerRequests(
        ViewportEngine& engine, ImageViewportPageRole role)
    {
        return engine.m_state->providerState
            .roles[role == ImageViewportPageRole::Secondary ? 1U : 0U]
            .provider.requests;
    }
    static ImageViewportInternal::ProviderFactsState& providerFacts(
        ViewportEngine& engine, ImageViewportPageRole role)
    {
        return engine.m_state->providerState
            .roles[role == ImageViewportPageRole::Secondary ? 1U : 0U]
            .provider.facts;
    }
};
#endif
