#include "viewportengine_p.h"
#include "viewportenginestate_p.h"

#include <utility>

ViewportRenderSynchronization ViewportEngine::beginRenderSynchronization(
    const RenderSynchronizationInput& input)
{
    return synchronizeViewportEngineRender(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->presentationState.presentation, m_state->renderCoordination });
}

ViewportEngineRenderCommitTransition ViewportEngine::acknowledgeRenderCommit(
    const RenderAcknowledgementInput& input)
{
    auto reduction = reduceViewportEngineRenderCommit(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, providerFactsView(), m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? playbackScheduleEffect()
                                        : ViewportPlaybackScheduleEffect {} };
}

ViewportEngineRenderFailureTransition ViewportEngine::acknowledgeRenderFailure(
    const RenderAcknowledgementInput& input)
{
    auto reduction = reduceViewportEngineRenderFailure(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? playbackScheduleEffect()
                                        : ViewportPlaybackScheduleEffect {},
        reduction.diagnostic };
}

ViewportEngineGeometryChangeTransition ViewportEngine::handleGeometryChanged(
    const ViewportEngineGeometryChangeInput& input)
{
    ViewportEngineGeometryChangeAccess access(
        m_state->requestState.request, m_state->displayState.display);
    auto reduction = reduceViewportEngineGeometryChange(input, std::move(access));
    ViewportEngineGeometryChangeTransition result;
    result.changes = reduction.changes;
    if (reduction.providerDemandGeometry) {
        result.providerEffects = restageProviderDemands(*reduction.providerDemandGeometry);
    }
    return result;
}
