#include "viewportengine_p.h"
#include "viewportenginestate_p.h"

#include <utility>

ViewportRenderSynchronization ViewportEngine::beginRenderSynchronization(
    const RenderSynchronizationInput& input)
{
    const GeometryInput current = currentGeometry(input.viewport);
    const PresentationGeometry::State currentState = geometryState(current);
    const ViewportEngineRenderSynchronizationInput operationInput { input.viewport.itemBounds.size(),
        input.viewport.itemBounds, PresentationGeometry::contentRect(currentState),
        PresentationGeometry::visibleImageRect(currentState), current,
        pendingGeometry(input.viewport) };
    return synchronizeViewportEngineRender(operationInput,
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
        reduction.changes.playbackPhase ? currentPlaybackSchedule()
                                        : ViewportPlaybackScheduleEffect {} };
}

ViewportEngineRenderFailureTransition ViewportEngine::acknowledgeRenderFailure(
    const RenderAcknowledgementInput& input)
{
    auto reduction = reduceViewportEngineRenderFailure(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? currentPlaybackSchedule()
                                        : ViewportPlaybackScheduleEffect {},
        reduction.diagnostic };
}

ViewportEngineGeometryChangeTransition ViewportEngine::handleGeometryChanged(
    const GeometryChangeInput& input)
{
    const ViewportEngineGeometryChangeInput operationInput { input.viewport.itemBounds,
        input.oldContentRect, input.oldVisibleImageRect, geometryState(input.viewport) };
    ViewportEngineGeometryChangeAccess access(
        m_state->requestState.request, m_state->displayState.display);
    auto reduction = reduceViewportEngineGeometryChange(operationInput, std::move(access));
    ViewportEngineGeometryChangeTransition result;
    result.changes = reduction.changes;
    if (reduction.providerDemandGeometry) {
        result.providerEffects = restageProviderDemands(*reduction.providerDemandGeometry);
    }
    return result;
}
