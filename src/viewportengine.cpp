#include "viewportengine_p.h"
#include "viewportengineassignmentoperations_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"
#include "viewportengineproviderprojection_p.h"
#include "viewportenginestate_p.h"

#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"

#include <utility>

namespace {
}

ViewportEngine::ViewportEngine()
    : m_state(std::make_unique<ViewportEngineCanonicalState>())
{
}

ViewportEngine::~ViewportEngine() = default;

ViewportEngineTransition ViewportEngine::handleResourcePressure(
    const ViewportEngineResourcePressureFact&)
{
    ViewportEngineTransition transition;
    auto& display = m_state->displayState.display;
    if (display.status != ImageViewportDisplayStatus::Retained) {
        return transition;
    }
    if (m_state->revisions.presentationRevision == 0) {
        m_state->revisions.presentationRevision = display.revision;
    }
    display.discardRetainedDisplay();
    transition.changes.displayState = true;
    transition.changes.geometryState = true;
    transition.changes.displayRevision = true;
    transition.changes.scheduleUpdate = true;
    return transition;
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

ViewportEngine::PendingPublication::PendingPublication(
    ViewportEngine* owner, ImageViewportInternal::ViewportChangeSet changes)
    : m_owner(owner)
    , m_changes(std::move(changes))
{
}

ViewportEngine::PendingPublication::PendingPublication(PendingPublication&& other) noexcept
    : m_owner(std::exchange(other.m_owner, nullptr))
    , m_changes(std::move(other.m_changes))
{
}

ViewportEngine::PendingPublication& ViewportEngine::PendingPublication::operator=(
    PendingPublication&& other) noexcept
{
    if (this != &other) {
        m_owner = std::exchange(other.m_owner, nullptr);
        m_changes = std::move(other.m_changes);
    }
    return *this;
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
        m_state->revisions.presentationRevision, m_state->revisions.snapshotRevision };
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

ViewportEngine::PendingPublication ViewportEngine::preparePublication(
    ImageViewportInternal::ViewportChangeSet changes)
{
    return PendingPublication(this, std::move(changes));
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::publish(PendingPublication publication)
{
    if (publication.m_owner != this) {
        qFatal("ViewportEngine pending publication owner mismatch");
    }
    publication.m_owner = nullptr;
    auto changes = std::move(publication.m_changes);
    if (changes.requestRevision) {
        m_state->requestState.request.requestRevision = allocateRevisionValue();
    }
    if (changes.displayRevision) {
        m_state->displayState.display.revision = allocateRevisionValue();
        if (m_state->displayState.display.status == ImageViewportDisplayStatus::Ready) {
            m_state->displayState.display.displayedPresentation
                = m_state->presentationState.presentation;
            m_state->displayState.display.displayedPresentationRevision
                = m_state->displayState.display.revision;
        }
    }
    if (changes.presentationRevision) {
        m_state->revisions.presentationRevision = allocateRevisionValue();
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
        || changes.commandRevision || changes.requestState || changes.displayState
        || changes.geometryState || changes.playbackPhase || changes.diagnostics) {
        m_state->revisions.snapshotRevision = allocateRevisionValue();
    }
    if (changes.scheduleUpdate) {
        m_state->renderCoordination.activeAttempt.reset();
    }
    return changes;
}

ImageViewportInternal::ViewportChangeSet ViewportEngine::publishChanges(
    ImageViewportInternal::ViewportChangeSet changes)
{
    return publish(preparePublication(std::move(changes)));
}

PresentationGeometry::State ViewportEngine::geometryState(const GeometryInput& input) const
{
    return projectViewportGeometryState(input, m_state->presentationState.presentation);
}

ViewportEngine::GeometryInput ViewportEngine::currentGeometry(
    ViewportEngineViewportInput input) const
{
    return projectViewportCurrentGeometry({ input.itemBounds, input.devicePixelRatio },
        { m_state->requestState.request, m_state->displayState.display });
}

ViewportEngine::GeometryInput ViewportEngine::pendingGeometry(
    ViewportEngineViewportInput input) const
{
    return projectViewportPendingGeometry({ input.itemBounds, input.devicePixelRatio },
        { m_state->requestState.request, m_state->displayState.display, providerFactsView() });
}

ViewportEngine::GeometryInput ViewportEngine::acceptedGeometry(
    ViewportEngineViewportInput input) const
{
    return projectViewportAcceptedGeometry({ input.itemBounds, input.devicePixelRatio },
        { m_state->requestState.request, providerFactsView() });
}

PresentationGeometry::State ViewportEngine::geometryState(ViewportEngineViewportInput input) const
{
    const ImageViewportInternal::PresentationState& displayedPresentation
        = m_state->displayState.display.status == ImageViewportDisplayStatus::Retained
        ? m_state->displayState.display.displayedPresentation
        : m_state->presentationState.presentation;
    return projectViewportGeometryState(currentGeometry(input), displayedPresentation);
}

ViewportRenderSnapshot ViewportEngine::renderSnapshot(
    const ViewportRenderSnapshotInput& input) const
{
    return projectViewportRenderSnapshot(input,
        { m_state->requestState.request, m_state->displayState.display,
            m_state->presentationState.presentation });
}

ViewportEnginePresentationTargetAssignmentResult ViewportEngine::assignPresentationTarget(
    const ViewportEnginePresentationTargetAssignmentRequest& input)
{
    if (!input.presentationTarget.isValid() || !input.transitionPolicy.isValid()) {
        return { rejectInvalidCommand(), m_state->requestState.presentationTarget };
    }
    ViewportEnginePresentationTargetAssignmentResult result;
    result.command
        = input.presentationTarget.isClear() ? accepted() : acceptedPreservingCommandDiagnostics();
    ViewportEnginePresentationTargetAssignmentAccess access(
        m_state->requestState.presentationTarget,
        m_state->requestState.nextPresentationTargetGeneration, m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation);
    const auto reduction = reduceViewportEnginePresentationTargetAssignment(
        { input.presentationTarget, input.transitionPolicy, input.primarySource,
            input.secondarySource, acceptedGeometry(input.viewport) },
        std::move(access));
    result.presentationTargetState = reduction.presentationTargetState;
    result.presentationTargetChanged = reduction.presentationTargetChanged;
    result.clear = reduction.clear;
    result.retainPreviousDisplay = reduction.retainPreviousDisplay;
    result.releaseDisplayedState = reduction.releaseDisplayedState;
    result.resetDisplayRequests = reduction.resetDisplayRequests;
    result.stopPlayback = reduction.stopPlayback;
    result.closeProviderSessions = reduction.closeProviderSessions;
    result.changes = reduction.changes;
    result.providerEffects = reduction.providerEffects;
    result.providerSessionOpenEffects = reduction.providerSessionOpenEffects;
    result.schedule = currentPlaybackSchedule();
    return result;
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
    const bool hadDiagnostic
        = m_state->commandState.reason != ImageViewportCommandReason::NoCommand;
    m_state->commandState.reason = ImageViewportCommandReason::NoCommand;
    if (hadDiagnostic) {
        m_state->commandState.revision = nextCommandRevision();
        return { ImageViewportCommandOutcome::Accepted, m_state->commandState.reason,
            m_state->commandState.revision, true };
    }
    return { ImageViewportCommandOutcome::Accepted, m_state->commandState.reason,
        m_state->commandState.revision, false };
}

ViewportEngineCommandResult ViewportEngine::acceptedPreservingCommandDiagnostics() const
{
    return { ImageViewportCommandOutcome::Accepted, m_state->commandState.reason,
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

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportEngine::setNextRevisionValueForTest(quint64 token)
{
    m_state->revisions.nextRevision = token == 0 ? 0 : token - 1;
    m_state->commandState.revision = {};
    m_state->commandState.publishedRevision = 0;
}
#endif
