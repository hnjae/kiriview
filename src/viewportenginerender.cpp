#include "viewportengine_p.h"
#include "viewportengineallocationoperations_p.h"
#include "viewportenginepresentationoperations_p.h"
#include "viewportenginerenderoperations_p.h"
#include "viewportenginestate_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <algorithm>
#include <cmath>
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
    target.targetPresentationRevision
        = target.targetPresentationRevision || source.targetPresentationRevision;
    target.adoptTargetPresentationRevision
        = target.adoptTargetPresentationRevision || source.adoptTargetPresentationRevision;
    target.scheduleUpdate = target.scheduleUpdate || source.scheduleUpdate;
}

bool identitiesEqual(ImageViewportInternal::PreparedPayloadIdentity lhs,
    ImageViewportInternal::PreparedPayloadIdentity rhs)
{
    return lhs.generation == rhs.generation && lhs.payloadId == rhs.payloadId;
}

bool targetSpreadsEqual(ImageViewportInternal::TargetSpreadIdentity lhs,
    ImageViewportInternal::TargetSpreadIdentity rhs)
{
    return lhs.generation == rhs.generation && lhs.requestId == rhs.requestId;
}

bool presentationsEqual(ImageViewportInternal::RenderPresentationIdentity lhs,
    ImageViewportInternal::RenderPresentationIdentity rhs)
{
    return lhs.revision == rhs.revision;
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
    if (!targetSpreadsEqual(acknowledgement.targetSpread, attempt.snapshot.targetSpread)
        || !presentationsEqual(acknowledgement.presentation, attempt.snapshot.presentation)) {
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
    observation.cause = ImageViewportInternal::InternalObservationCause::StaleRenderAcknowledgement;
    observation.identity.renderAttempt = fact.acknowledgement.attempt;
    if (!fact.acknowledgement.rolePayloads.isEmpty()) {
        const auto& payload = fact.acknowledgement.rolePayloads.constFirst();
        observation.identity.roleValid = true;
        observation.identity.role = payload.role;
        observation.identity.generation = payload.preparedPayload.generation;
        observation.identity.requestId = fact.acknowledgement.targetSpread.requestId;
        observation.identity.payloadId = payload.preparedPayload.payloadId;
    }
    return observation;
}

ViewportEngineViewportState normalizedViewportState(ViewportEngineViewportState viewport)
{
    const double width = viewport.itemBounds.width();
    const double height = viewport.itemBounds.height();
    viewport.itemBounds
        = std::isfinite(width) && std::isfinite(height) && width > 0.0 && height > 0.0
        ? QRectF(0.0, 0.0, width, height)
        : QRectF();
    if (!std::isfinite(viewport.devicePixelRatio) || viewport.devicePixelRatio <= 0.0) {
        viewport.devicePixelRatio = 1.0;
    }
    return viewport;
}

bool requestDomainEqual(
    const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
{
    return lhs.request() == rhs.request() && lhs.primary().present() == rhs.primary().present()
        && lhs.primary().sequence() == rhs.primary().sequence()
        && lhs.primary().request() == rhs.primary().request()
        && lhs.primary().metadata() == rhs.primary().metadata()
        && lhs.secondary().present() == rhs.secondary().present()
        && lhs.secondary().sequence() == rhs.secondary().sequence()
        && lhs.secondary().request() == rhs.secondary().request()
        && lhs.secondary().metadata() == rhs.secondary().metadata()
        && lhs.diagnostics().errorString() == rhs.diagnostics().errorString();
}

bool displayDomainEqual(
    const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
{
    return lhs.display() == rhs.display() && lhs.primary().display() == rhs.primary().display()
        && lhs.primary().geometry() == rhs.primary().geometry()
        && lhs.secondary().display() == rhs.secondary().display()
        && lhs.secondary().geometry() == rhs.secondary().geometry()
        && lhs.diagnostics().warningString() == rhs.diagnostics().warningString();
}

bool acceptedProjectionEqual(
    const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
{
    const auto roleEqual = [](const ImageViewportRoleGeometrySnapshot& left,
                               const ImageViewportRoleGeometrySnapshot& right) {
        return left.acceptedPageRect() == right.acceptedPageRect()
            && left.acceptedItemRect() == right.acceptedItemRect()
            && left.acceptedVisiblePageRect() == right.acceptedVisiblePageRect();
    };
    return roleEqual(lhs.primary().geometry(), rhs.primary().geometry())
        && roleEqual(lhs.secondary().geometry(), rhs.secondary().geometry())
        && lhs.presentation().zoomPercent() == rhs.presentation().zoomPercent();
}

bool displayedProjectionEqual(
    const ImageViewportStateSnapshot& lhs, const ImageViewportStateSnapshot& rhs)
{
    const auto roleEqual = [](const ImageViewportRoleGeometrySnapshot& left,
                               const ImageViewportRoleGeometrySnapshot& right) {
        return left.displayedPageRect() == right.displayedPageRect()
            && left.displayedItemRect() == right.displayedItemRect()
            && left.displayedVisiblePageRect() == right.displayedVisiblePageRect();
    };
    return roleEqual(lhs.primary().geometry(), rhs.primary().geometry())
        && roleEqual(lhs.secondary().geometry(), rhs.secondary().geometry())
        && lhs.display().contentRect() == rhs.display().contentRect()
        && lhs.display().contentSize() == rhs.display().contentSize()
        && lhs.display().contentPosition() == rhs.display().contentPosition()
        && lhs.display().maximumContentPosition() == rhs.display().maximumContentPosition()
        && lhs.display().visibleSpreadRect() == rhs.display().visibleSpreadRect();
}

bool clampContentPosition(ImageViewportInternal::PresentationState& presentation,
    const ViewportEngineGeometryInput& geometry)
{
    if (!geometry.primaryPresent || geometry.itemBounds.isEmpty()) {
        return false;
    }
    const PresentationGeometry::State projected
        = projectViewportGeometryState(geometry, presentation);
    if (PresentationGeometry::contentRect(projected).isEmpty()) {
        return false;
    }
    const QPointF clamped = PresentationGeometry::contentPosition(projected);
    if (presentation.contentPosition == clamped) {
        return false;
    }
    presentation.contentPosition = clamped;
    return true;
}
}

ViewportRenderAttempt ViewportEngine::beginRenderSynchronization()
{
    const GeometryInput current = currentGeometry();
    const PresentationGeometry::State currentState = geometryState(current);
    const ViewportEngineRenderSynchronizationInput operationInput {
        m_state->revisions.targetPresentationRevision,
        m_state->displayState.display.displayedPresentationRevision,
        m_state->viewport.itemBounds.size(), m_state->viewport.itemBounds,
        PresentationGeometry::contentRect(currentState),
        PresentationGeometry::visibleImageRect(currentState), current, pendingGeometry()
    };
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
        context.pendingRefinementCommit,
        context.pendingPrimaryRefinementCommit,
        context.pendingSecondaryRefinementCommit,
        !context.pendingTargetCommit && !context.pendingRefinementCommit && input.fact.imagePresent,
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
        rebuildViewportEnginePayloadAllocation(
            m_state->requestState.request, m_state->displayState.display);
    } else if (input.fact.outcome == ViewportRenderHostFact::Outcome::Committed
        && input.fact.imagePresent
        && (context.pendingTargetCommit || context.pendingRefinementCommit)) {
        auto reduction = reduceViewportEngineRenderCommit(acknowledgementInput,
            { m_state->requestState.request, m_state->displayState.display,
                m_state->playbackState.playback, providerFactsView() });
        result.changes = reduction.changes;
        if (reduction.changes.displayRevision
            && m_state->displayState.display.status == ImageViewportDisplayStatus::Ready) {
            result.changes.adoptTargetPresentationRevision = true;
        }
        result.observations = reduction.observations;
        const auto allocation = rebuildViewportEnginePayloadAllocation(
            m_state->requestState.request, m_state->displayState.display);
        if (context.pendingTargetCommit && allocation.roleBudgetsIncreased) {
            const GeometryInput geometry { context.geometryState.hasReadyDisplay,
                context.geometryState.itemBounds, context.geometryState.primaryImageSize,
                context.geometryState.secondaryImageSize, context.geometryState.devicePixelRatio };
            result.providerEffects = restageProviderDemands(geometry);
        }
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

ViewportEngineTransition ViewportEngine::handleViewportChanged(ViewportEngineViewportState viewport)
{
    viewport = normalizedViewportState(viewport);
    if (viewport == m_state->viewport) {
        return {};
    }

    ViewportEngineTransition result;
    const ImageViewportStateSnapshot before = snapshot();
    const PresentationGeometry::State oldDisplayedGeometry = geometryState();
    const QRectF oldContentRect = PresentationGeometry::contentRect(oldDisplayedGeometry);
    const QRectF oldVisibleImageRect = PresentationGeometry::visibleImageRect(oldDisplayedGeometry);
    const bool geometryInputChanged = viewport.itemBounds.x() != m_state->viewport.itemBounds.x()
        || viewport.itemBounds.y() != m_state->viewport.itemBounds.y()
        || viewport.itemBounds.width() != m_state->viewport.itemBounds.width()
        || viewport.itemBounds.height() != m_state->viewport.itemBounds.height()
        || viewport.devicePixelRatio != m_state->viewport.devicePixelRatio;
    const bool availabilityChanged = viewport.renderAvailable != m_state->viewport.renderAvailable;
    m_state->viewport = viewport;

    std::optional<ViewportEngineGeometryInput> providerDemandGeometry;
    if (geometryInputChanged) {
        const auto transitionChanges
            = resolveViewportEnginePendingPresentationTargetTransition(rawAcceptedGeometry(),
                m_state->requestState.presentationTarget, m_state->presentationState.presentation,
                m_state->displayState.display.hasReadyDisplay(
                    m_state->requestState.request.roles[0].source.facts.present));
        clampContentPosition(m_state->presentationState.presentation, acceptedGeometry());
        if (m_state->displayState.display.status == ImageViewportDisplayStatus::Retained) {
            clampContentPosition(
                m_state->displayState.display.displayedPresentation, currentGeometry());
        }
        const ViewportEngineGeometryChangeInput operationInput { viewport.itemBounds,
            oldContentRect, oldVisibleImageRect, geometryState(),
            m_state->presentationState.presentation.exactnessPreference };
        ViewportEngineGeometryChangeAccess access(
            m_state->requestState.request, m_state->displayState.display);
        auto reduction = reduceViewportEngineGeometryChange(operationInput, std::move(access));
        result.changes = reduction.changes;
        mergeChanges(result.changes, transitionChanges);
        if (reduction.providerDemandGeometry) {
            providerDemandGeometry = acceptedGeometry();
        }
    }

    if (availabilityChanged) {
        auto& request = m_state->requestState.request;
        const auto& display = m_state->displayState.display;
        const bool secondaryRequired
            = request.roles[1].sequence && request.roles[1].activeRequest.target.frame >= 0;
        const bool completePayload = display.roles[0].pendingRenderPayload.commitPending
            && !display.roles[0].pendingRenderPayload.image.isNull()
            && (!secondaryRequired || !display.roles[1].pendingRenderPayload.image.isNull());
        if (!viewport.renderAvailable && completePayload
            && request.status == ImageViewportRequestStatus::Loading
            && request.reason == ImageViewportRequestReason::UploadPending) {
            request.reason = ImageViewportRequestReason::RenderWaiting;
        }
    }

    const ImageViewportStateSnapshot beforeProjectionIdentity = snapshot();
    const bool targetProjectionChanged = m_state->requestState.presentationTarget.generation != 0
        && !acceptedProjectionEqual(before, beforeProjectionIdentity);
    const bool displayedProjectionChanged
        = !displayedProjectionEqual(before, beforeProjectionIdentity);
    if (targetProjectionChanged) {
        advanceTargetPresentationRevision();
    }
    auto& display = m_state->displayState.display;
    if (display.status == ImageViewportDisplayStatus::Ready && targetProjectionChanged) {
        display.displayedPresentation = m_state->presentationState.presentation;
        display.displayedPresentationRevision = m_state->revisions.targetPresentationRevision;
    } else if (display.status == ImageViewportDisplayStatus::Retained
        && displayedProjectionChanged) {
        display.displayedPresentationRevision = allocateRevisionValue();
    }

    if (providerDemandGeometry) {
        const auto effects = restageProviderDemands(*providerDemandGeometry);
        appendProviderTransport(
            result.providerAfterPublication, effects[0], ImageViewportPageRole::Primary);
        appendProviderTransport(
            result.providerAfterPublication, effects[1], ImageViewportPageRole::Secondary);
    }

    const ImageViewportStateSnapshot after = snapshot();
    const bool requestChanged = !requestDomainEqual(before, after);
    const bool displayChanged = !displayDomainEqual(before, after);
    const bool presentationChanged = before.presentation() != after.presentation();
    result.changes.requestState = requestChanged;
    result.changes.displayState = displayChanged;
    result.changes.geometryState
        = !acceptedProjectionEqual(before, after) || !displayedProjectionEqual(before, after);
    result.changes.diagnostics = before.diagnostics() != after.diagnostics();
    result.changes.requestRevision = requestChanged;
    result.changes.displayRevision = displayChanged;
    result.changes.presentationRevision = presentationChanged;
    result.changes.targetPresentationRevision = targetProjectionChanged;
    result.changes.adoptTargetPresentationRevision = false;
    result.changes.scheduleUpdate = true;
    return result;
}
