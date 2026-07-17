#include "viewportengine_p.h"
#include "viewportengineallocationoperations_p.h"
#include "viewportengineassignmentoperations_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"
#include "viewportengineproviderprojection_p.h"
#include "viewportenginestate_p.h"
#include "viewportprovidertransporteffects_p.h"

#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"

#include <utility>

namespace {
}

ViewportEngine::ViewportEngine()
    : m_state(std::make_unique<ViewportEngineCanonicalState>())
{
    m_state->requestState.request.requestRevision = allocateRevisionValue();
    m_state->displayState.display.revision = allocateRevisionValue();
    m_state->revisions.presentationRevision = allocateRevisionValue();
    m_state->commandState.publishedRevision = allocateRevisionValue();
    m_state->commandState.revision = ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(
        m_state->commandState.publishedRevision);
    m_state->revisions.snapshotRevision = allocateRevisionValue();
}

ViewportEngine::~ViewportEngine() = default;

ViewportEngineTransition ViewportEngine::handleResourcePressure()
{
    ViewportEngineTransitionDraft transition;
    auto& display = m_state->displayState.display;
    if (display.status != ImageViewportDisplayStatus::Retained) {
        return finalizeTransition(std::move(transition));
    }
    const bool warningBefore = display.hasActiveRenderQualityFallback(
        m_state->requestState.request.sequenceGeneration, m_state->presentationState.presentation);
    display.discardRetainedDisplay();
    rebuildViewportEnginePayloadAllocation(m_state->requestState.request, display);
    transition.changes.displayState = true;
    transition.changes.geometryState = true;
    transition.changes.diagnostics = warningBefore;
    transition.changes.displayRevision = true;
    transition.changes.scheduleUpdate = true;
    const auto effects = restageProviderDemands();
    appendProviderTransport(
        transition.providerTransport, effects[0], ImageViewportPageRole::Primary);
    appendProviderTransport(
        transition.providerTransport, effects[1], ImageViewportPageRole::Secondary);
    return finalizeTransition(std::move(transition));
}

QSet<quint64> ViewportEngine::providerFrameLeaseIds() const
{
    QSet<quint64> leases;
    for (const auto& role : m_state->displayState.display.roles) {
        if (role.displayedPayload.providerFrameLeaseId != 0) {
            leases.insert(role.displayedPayload.providerFrameLeaseId);
        }
        if (role.pendingRenderPayload.providerFrameLeaseId != 0) {
            leases.insert(role.pendingRenderPayload.providerFrameLeaseId);
        }
    }
    return leases;
}

ViewportEngineCommandDiagnostics ViewportEngine::commandDiagnostics() const
{
    return { m_state->commandState.reason, m_state->commandState.revision };
}

ViewportEnginePresentationTargetState ViewportEngine::presentationTargetState() const
{
    return m_state->requestState.presentationTarget;
}

ViewportEngineProviderFactsView ViewportEngine::providerFactsView() const
{
    return { m_state->providerState.roles[0].provider.facts,
        m_state->providerState.roles[1].provider.facts };
}

ViewportEngineSnapshotStateAccess ViewportEngine::snapshotAccess() const
{
    return { m_state->requestState.request, m_state->playbackState.playback,
        m_state->displayState.display, providerFactsView(), m_state->presentationState.presentation,
        m_state->requestState.presentationTarget, m_state->commandState.reason,
        m_state->commandState.revision, m_state->commandState.publishedRevision,
        m_state->revisions.presentationRevision, m_state->revisions.targetPresentationRevision,
        m_state->revisions.snapshotRevision };
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
ImageViewportInternal::DisplayState& ViewportEngine::displayState()
{
    return m_state->displayState.display;
}

const ImageViewportInternal::DisplayState& ViewportEngine::displayState() const
{
    return m_state->displayState.display;
}

ImageViewportInternal::RequestState& ViewportEngine::requestState()
{
    return m_state->requestState.request;
}

const ImageViewportInternal::RequestState& ViewportEngine::requestState() const
{
    return m_state->requestState.request;
}

ImageViewportInternal::PlaybackState& ViewportEngine::playbackState()
{
    return m_state->playbackState.playback;
}

const ImageViewportInternal::PlaybackState& ViewportEngine::playbackState() const
{
    return m_state->playbackState.playback;
}
#endif

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
const ImageViewportInternal::PresentationState& ViewportEngine::presentationState() const
{
    return m_state->presentationState.presentation;
}
#endif

ImageViewportInternal::ViewportChangeSet ViewportEngine::publishChanges(
    ImageViewportInternal::ViewportChangeSet changes)
{
    if (changes.requestRevision) {
        m_state->requestState.request.requestRevision = allocateRevisionValue();
    }
    if (changes.displayRevision) {
        m_state->displayState.display.revision = allocateRevisionValue();
    }
    if (changes.presentationRevision) {
        m_state->revisions.presentationRevision = allocateRevisionValue();
    }
    if (changes.adoptTargetPresentationRevision
        && m_state->displayState.display.status == ImageViewportDisplayStatus::Ready) {
        m_state->displayState.display.displayedPresentation
            = m_state->presentationState.presentation;
        m_state->displayState.display.displayedPresentationRevision
            = m_state->revisions.targetPresentationRevision;
    }
    if (changes.commandRevision) {
        m_state->commandState.publishedRevision = changes.commandRevisionValue != 0
            ? changes.commandRevisionValue
            : allocateRevisionValue();
        m_state->commandState.revision
            = ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(
                m_state->commandState.publishedRevision);
    }
    if (changes.requestRevision || changes.displayRevision || changes.presentationRevision
        || changes.targetPresentationRevision || changes.commandRevision || changes.requestState
        || changes.displayState || changes.geometryState || changes.playbackPhase
        || changes.diagnostics) {
        m_state->revisions.snapshotRevision = allocateRevisionValue();
    }
    if (changes.scheduleUpdate) {
        m_state->renderCoordination.activeAttempt.reset();
    }
    return changes;
}

ViewportEngineTransition ViewportEngine::finalizeTransition(ViewportEngineTransitionDraft draft)
{
    draft.changes = publishChanges(draft.changes);
    return ViewportEngineTransition(std::move(draft));
}

ViewportEngineCommandTransition ViewportEngine::finalizeCommandTransition(
    const ViewportEngineCommandResult& command, ViewportEngineTransitionDraft draft)
{
    if (command.commandRevisionChanged) {
        draft.changes.commandRevision = true;
        draft.changes.commandRevisionValue
            = ImageViewportInternal::RevisionTokenPrivateAccess::value(command.commandRevision);
        draft.changes.diagnostics = true;
    }
    return ViewportEngineCommandTransition(command.outcome, finalizeTransition(std::move(draft)));
}

PresentationGeometry::State ViewportEngine::geometryState(const GeometryInput& input) const
{
    return projectViewportGeometryState(input, m_state->presentationState.presentation);
}

ViewportEngine::GeometryInput ViewportEngine::currentGeometry() const
{
    const auto& input = m_state->viewport;
    return projectViewportCurrentGeometry(
        { input.itemBounds, input.devicePixelRatio, input.renderAvailable },
        { m_state->requestState.request, m_state->displayState.display });
}

ViewportEngine::GeometryInput ViewportEngine::pendingGeometry() const
{
    const auto& input = m_state->viewport;
    return projectViewportPendingGeometry(
        { input.itemBounds, input.devicePixelRatio, input.renderAvailable },
        { m_state->requestState.request, providerFactsView() });
}

ViewportEngine::GeometryInput ViewportEngine::acceptedGeometry() const
{
    GeometryInput result = rawAcceptedGeometry();
    const auto& pending = m_state->requestState.presentationTarget.pendingPresentationTransition;
    const auto positive
        = [](QSizeF size) { return size.isValid() && size.width() > 0.0 && size.height() > 0.0; };
    if (pending.isValid()
        && (!result.primaryPresent || !positive(result.primarySize)
            || (m_state->requestState.presentationTarget.acceptedRoleSet.secondary()
                && !positive(result.secondarySize)))) {
        result.primaryPresent = false;
        result.primarySize = {};
        result.secondarySize = {};
    }
    return result;
}

ViewportEngine::GeometryInput ViewportEngine::rawAcceptedGeometry() const
{
    const auto& input = m_state->viewport;
    return projectViewportAcceptedGeometry(
        { input.itemBounds, input.devicePixelRatio, input.renderAvailable },
        { m_state->requestState.request, providerFactsView() });
}

PresentationGeometry::State ViewportEngine::geometryState() const
{
    const ImageViewportInternal::PresentationState& displayedPresentation
        = m_state->displayState.display.status == ImageViewportDisplayStatus::Retained
        ? m_state->displayState.display.displayedPresentation
        : m_state->presentationState.presentation;
    return projectViewportGeometryState(currentGeometry(), displayedPresentation);
}

ViewportRenderSnapshot ViewportEngine::renderSnapshot(
    const ViewportRenderSnapshotInput& input) const
{
    return projectViewportRenderSnapshot(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->presentationState.presentation });
}

ViewportEngineCommandTransition ViewportEngine::assignPresentationTarget(
    const ViewportEnginePresentationTargetAssignmentRequest& input)
{
    ViewportEnginePresentationTargetAssignmentInput operationInput { input.presentationTarget,
        input.transitionPolicy, input.primarySource, input.secondarySource, acceptedGeometry() };
    if (!operationInput.presentationTarget.isClear() && !operationInput.primarySource.sequence) {
        operationInput.primarySource = ImageViewportInternal::factorySequenceSource(
            operationInput.presentationTarget.primary());
    }
    if (!operationInput.presentationTarget.isClear() && !operationInput.secondarySource.sequence) {
        operationInput.secondarySource = ImageViewportInternal::factorySequenceSource(
            operationInput.presentationTarget.secondary());
    }
    if (!validateViewportEnginePresentationTargetAssignment(operationInput,
            m_state->requestState.presentationTarget, m_state->requestState.request,
            m_state->providerState.roles)) {
        return finalizeCommandTransition(rejectInvalidCommand(), {});
    }
    ViewportEnginePresentationTargetAssignmentAccess access(
        m_state->requestState.presentationTarget,
        m_state->requestState.nextPresentationTargetGeneration, m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation);
    const auto reduction = reduceViewportEnginePresentationTargetAssignment(
        std::move(operationInput), std::move(access));
    ViewportEnginePayloadAllocationRebuildResult allocation;
    if (reduction.presentationTargetChanged) {
        allocation = rebuildViewportEnginePayloadAllocation(
            m_state->requestState.request, m_state->displayState.display);
    }
    if (reduction.presentationTargetChanged) {
        m_state->revisions.targetPresentationRevision
            = reduction.clear ? 0 : advanceTargetPresentationRevision();
    }
    const ViewportEngineCommandResult command
        = reduction.presentationTargetChanged ? accepted() : acceptedPreservingCommandDiagnostics();
    ViewportEngineTransitionDraft result;
    result.changes = reduction.changes;
    if (allocation.retainedDisplayDiscarded) {
        result.changes.displayState = true;
        result.changes.geometryState = true;
        result.changes.displayRevision = true;
    }
    result.changes.targetPresentationRevision = reduction.presentationTargetChanged;
    appendProviderTransport(
        result.providerTransport, reduction.providerEffects[0], ImageViewportPageRole::Primary);
    appendProviderTransport(
        result.providerTransport, reduction.providerEffects[1], ImageViewportPageRole::Secondary);
    for (const auto& effect : reduction.providerSessionOpenEffects) {
        if (effect.openSession) {
            result.providerTransport.append(effect.command);
        }
    }
    result.playbackSchedule = currentPlaybackSchedule();
    return finalizeCommandTransition(command, std::move(result));
}
ViewportEngineCommandResult ViewportEngine::rejectInvalidCommand()
{
    return rejected(
        ImageViewportCommandOutcome::Invalid, ImageViewportCommandReason::InvalidRequest);
}

ViewportEngineCommandResult ViewportEngine::rejected(
    ImageViewportCommandOutcome outcome, ImageViewportCommandReason reason)
{
    m_state->commandState.reason = reason;
    m_state->commandState.revision = nextCommandRevision();
    return { outcome, reason, m_state->commandState.revision, true };
}

ViewportEngineCommandResult ViewportEngine::accepted()
{
    m_state->commandState.reason = ImageViewportCommandReason::NoCommand;
    m_state->commandState.revision = nextCommandRevision();
    return { ImageViewportCommandOutcome::Accepted, m_state->commandState.reason,
        m_state->commandState.revision, true };
}

ViewportEngineCommandResult ViewportEngine::acceptedPreservingCommandDiagnostics() const
{
    return { ImageViewportCommandOutcome::Accepted, ImageViewportCommandReason::NoCommand,
        m_state->commandState.revision, false };
}

RevisionToken ViewportEngine::nextCommandRevision()
{
    return ImageViewportInternal::RevisionTokenPrivateAccess::fromValue(allocateRevisionValue());
}

quint64 ViewportEngine::allocateRevisionValue()
{
    if (m_state->revisions.nextRevision == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport revision token allocator exhausted");
    }
    return ++m_state->revisions.nextRevision;
}

quint64 ViewportEngine::advanceTargetPresentationRevision()
{
    m_state->revisions.targetPresentationRevision = allocateRevisionValue();
    return m_state->revisions.targetPresentationRevision;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportEngine::setNextRevisionValueForTest(quint64 token)
{
    m_state->revisions.nextRevision = token == 0 ? 0 : token - 1;
    m_state->commandState.revision = {};
    m_state->commandState.publishedRevision = 0;
}
#endif
