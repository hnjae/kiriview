#include "viewportengine_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportenginestate_p.h"

#include <utility>

namespace {
ViewportEngineRenderAcknowledgementInput operationInput(
    const ViewportEngineRenderAcknowledgementRequest& input)
{
    return { input.acknowledgement, input.renderedImagePresent, input.synchronizationAttempt,
        input.pendingTargetCommit, input.pendingSecondaryProviderCommit, input.preparedPayload,
        input.oldDisplayStatus, input.oldContentRect, input.oldVisibleImageRect,
        input.geometryState };
}
}

ViewportRenderSynchronization ViewportEngine::beginRenderSynchronization(
    const ViewportEngineRenderSynchronizationRequest& input)
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
    const ViewportEngineRenderAcknowledgementRequest& input)
{
    auto reduction = reduceViewportEngineRenderCommit(operationInput(input),
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, providerFactsView(), m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? currentPlaybackSchedule()
                                        : ViewportPlaybackScheduleEffect {},
        reduction.observations };
}

ViewportEngineRenderFailureTransition ViewportEngine::acknowledgeRenderFailure(
    const ViewportEngineRenderAcknowledgementRequest& input)
{
    auto reduction = reduceViewportEngineRenderFailure(operationInput(input),
        { m_state->requestState.request, m_state->displayState.display,
            m_state->playbackState.playback, m_state->renderCoordination });
    return { reduction.changes,
        reduction.changes.playbackPhase ? currentPlaybackSchedule()
                                        : ViewportPlaybackScheduleEffect {},
        reduction.diagnostic, reduction.observations };
}

ViewportEngineRenderQualityFallbackTransition ViewportEngine::handleRenderQualityFallback(
    const ViewportRenderQualityFallbackFact& fact)
{
    ViewportEngineRenderQualityFallbackTransition result;
    if (fact.synchronizationAttempt == 0
        || fact.synchronizationAttempt != m_state->renderCoordination.nextSynchronizationAttempt) {
        ImageViewportInternal::InternalObservation observation;
        observation.subsystem = ImageViewportInternal::InternalObservationSubsystem::Engine;
        observation.category = ImageViewportInternal::InternalObservationCategory::StaleDrop;
        observation.cause
            = ImageViewportInternal::InternalObservationCause::StaleRenderQualityFallback;
        observation.identity.renderAttempt = fact.synchronizationAttempt;
        result.observations.append(observation);
        return result;
    }
    const QString warning = fact.smoothingUnavailable || fact.mipmapUnavailable
        ? QStringLiteral("requested rendering quality is unavailable on the active backend")
        : QString();
    if (m_state->requestState.request.warningString == warning) {
        return result;
    }
    m_state->requestState.request.warningString = warning;
    result.changes.diagnostics = true;
    result.changes.displayRevision = true;
    return result;
}

ViewportEngineGeometryChangeTransition ViewportEngine::handleGeometryChanged(
    const ViewportEngineGeometryChangeRequest& input)
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
