#include "viewportengine_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportenginestate_p.h"

#include <algorithm>
#include <utility>

namespace {
void mergeChanges(ImageViewportInternal::ViewportChangeSet& target,
    const ImageViewportInternal::ViewportChangeSet& source)
{
    target.requestState = target.requestState || source.requestState;
    target.displayState = target.displayState || source.displayState;
    target.geometryState = target.geometryState || source.geometryState;
    target.playbackPhase = target.playbackPhase || source.playbackPhase;
    target.diagnostics = target.diagnostics || source.diagnostics;
    target.displayRevision = target.displayRevision || source.displayRevision;
    target.requestRevision = target.requestRevision || source.requestRevision;
    target.commandRevision = target.commandRevision || source.commandRevision;
    target.presentationRevision = target.presentationRevision || source.presentationRevision;
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
}

bool identitiesEqual(ImageViewportInternal::PreparedPayloadIdentity lhs,
    ImageViewportInternal::PreparedPayloadIdentity rhs)
{
    return lhs.generation == rhs.generation && lhs.requestId == rhs.requestId
        && lhs.payloadId == rhs.payloadId;
}

QVector<ViewportRenderRolePayload> expectedPayloads(const ViewportRenderAttempt& attempt)
{
    QVector<ViewportRenderRolePayload> result;
    result.reserve(attempt.snapshot.imageLayers.size());
    for (const ViewportRenderLayer& layer : attempt.snapshot.imageLayers) {
        result.append({ layer.role, layer.preparedPayload.identity() });
    }
    return result;
}

bool acknowledgementMatchesAttempt(
    const ViewportRenderHostFact& fact, const ViewportRenderAttempt& attempt)
{
    const ViewportRenderAcknowledgement& acknowledgement = fact.acknowledgement;
    if (acknowledgement.attempt == 0 || acknowledgement.attempt != attempt.attempt) {
        return false;
    }
    const QVector<ViewportRenderRolePayload> expected = expectedPayloads(attempt);
    if (fact.outcome == ViewportRenderHostFact::Outcome::Failed) {
        const auto expectedFailed = std::find_if(expected.cbegin(), expected.cend(),
            [&](const auto& payload) { return payload.role == acknowledgement.failedRole; });
        const auto actualFailed = std::find_if(acknowledgement.rolePayloads.cbegin(),
            acknowledgement.rolePayloads.cend(),
            [&](const auto& payload) { return payload.role == acknowledgement.failedRole; });
        return expectedFailed != expected.cend()
            && actualFailed != acknowledgement.rolePayloads.cend()
            && identitiesEqual(actualFailed->preparedPayload, expectedFailed->preparedPayload);
    }
    if (acknowledgement.rolePayloads.size() != expected.size()) {
        return false;
    }
    for (qsizetype index = 0; index < expected.size(); ++index) {
        const auto& actual = acknowledgement.rolePayloads.at(index);
        const auto& wanted = expected.at(index);
        if (actual.role != wanted.role
            || !identitiesEqual(actual.preparedPayload, wanted.preparedPayload)) {
            return false;
        }
    }
    return true;
}

ImageViewportInternal::InternalObservation staleRenderFactObservation(
    const ViewportRenderHostFact& fact)
{
    ImageViewportInternal::InternalObservation observation;
    observation.subsystem = ImageViewportInternal::InternalObservationSubsystem::Engine;
    observation.category = ImageViewportInternal::InternalObservationCategory::StaleDrop;
    observation.cause
        = ImageViewportInternal::InternalObservationCause::StaleRenderAcknowledgement;
    observation.identity.renderAttempt = fact.acknowledgement.attempt;
    if (!fact.acknowledgement.rolePayloads.isEmpty()) {
        const auto& payload = fact.acknowledgement.rolePayloads.constFirst();
        observation.identity.roleValid = true;
        observation.identity.role = payload.role;
        observation.identity.generation = payload.preparedPayload.generation;
        observation.identity.requestId = payload.preparedPayload.requestId;
        observation.identity.payloadId = payload.preparedPayload.payloadId;
    }
    return observation;
}
}

ViewportRenderAttempt ViewportEngine::beginRenderSynchronization(
    const ViewportEngineRenderSynchronizationRequest& input)
{
    const GeometryInput current = currentGeometry(input.viewport);
    const PresentationGeometry::State currentState = geometryState(current);
    const ViewportEngineRenderSynchronizationInput operationInput { input.viewport.itemBounds.size(),
        input.viewport.itemBounds, PresentationGeometry::contentRect(currentState),
        PresentationGeometry::visibleImageRect(currentState), current,
        pendingGeometry(input.viewport) };
    auto context = synchronizeViewportEngineRender(operationInput,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->presentationState.presentation, m_state->renderCoordination });
    const ViewportRenderAttempt attempt = context.attempt;
    m_state->renderCoordination.activeAttempt = std::move(context);
    return attempt;
}

ViewportEngineRenderHostTransition ViewportEngine::handleRenderHostFact(
    const ViewportEngineRenderHostFactRequest& input)
{
    ViewportEngineRenderHostTransition result;
    const auto& active = m_state->renderCoordination.activeAttempt;
    if (!active || !acknowledgementMatchesAttempt(input.fact, active->attempt)) {
        result.observations.append(staleRenderFactObservation(input.fact));
        return result;
    }

    const auto context = *active;
    const ViewportEngineRenderAcknowledgementInput acknowledgementInput {
        input.fact.acknowledgement,
        input.fact.imagePresent,
        context.attempt.attempt,
        context.pendingTargetCommit,
        context.pendingSecondaryProviderCommit,
        context.preparedPayload,
        context.oldDisplayStatus,
        context.oldContentRect,
        context.oldVisibleImageRect,
        context.geometryState,
    };

    if (input.fact.outcome == ViewportRenderHostFact::Outcome::Failed) {
        auto reduction = reduceViewportEngineRenderFailure(acknowledgementInput,
            { m_state->requestState.request, m_state->displayState.display,
                m_state->playbackState.playback });
        result.changes = reduction.changes;
        result.diagnostic = reduction.diagnostic;
        result.observations = reduction.observations;
    } else if (input.fact.outcome == ViewportRenderHostFact::Outcome::Committed
        && input.fact.imagePresent
        && m_state->displayState.display.roles[0].pendingRenderPayload.commitPending) {
        auto reduction = reduceViewportEngineRenderCommit(acknowledgementInput,
            { m_state->requestState.request, m_state->displayState.display,
                m_state->playbackState.playback, providerFactsView() });
        result.changes = reduction.changes;
        result.observations = reduction.observations;
    }

    if (input.fact.outcome == ViewportRenderHostFact::Outcome::Committed) {
        const auto& quality = input.fact.qualityFallback;
        const QString warning = quality.smoothingUnavailable || quality.mipmapUnavailable
            ? QStringLiteral("requested rendering quality is unavailable on the active backend")
            : QString();
        if (m_state->requestState.request.warningString != warning) {
            m_state->requestState.request.warningString = warning;
            result.changes.diagnostics = true;
            result.changes.displayRevision = true;
        }
    }

    m_state->renderCoordination.activeAttempt.reset();
    if (result.changes.playbackPhase) {
        result.playbackSchedule = currentPlaybackSchedule();
    }
    return result;
}

ViewportEngineGeometryChangeTransition ViewportEngine::handleGeometryChanged(
    const ViewportEngineGeometryChangeRequest& input)
{
    const GeometryInput rawGeometry = rawAcceptedGeometry(input.viewport);
    const auto transitionChanges
        = resolveViewportEnginePendingPresentationTargetTransition(rawGeometry,
            m_state->requestState.presentationTarget, m_state->presentationState.presentation,
            m_state->displayState.display.hasReadyDisplay(
                m_state->requestState.request.roles[0].source.facts.present));
    const ViewportEngineGeometryChangeInput operationInput { input.viewport.itemBounds,
        input.oldContentRect, input.oldVisibleImageRect, geometryState(input.viewport) };
    ViewportEngineGeometryChangeAccess access(
        m_state->requestState.request, m_state->displayState.display);
    auto reduction = reduceViewportEngineGeometryChange(operationInput, std::move(access));
    ViewportEngineGeometryChangeTransition result;
    result.changes = reduction.changes;
    mergeChanges(result.changes, transitionChanges);
    if (reduction.providerDemandGeometry) {
        result.providerEffects = restageProviderDemands(*reduction.providerDemandGeometry);
    }
    return result;
}
