// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengine_p.h"
#include "viewportengineallocationoperations_p.h"
#include "viewportengineassignmentoperations_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineprojection_p.h"
#include "viewportengineproviderprojection_p.h"
#include "viewportenginestate_p.h"
#include "viewportenginetargetspreadterminaloperations_p.h"
#include "viewportprovidertransporteffects_p.h"

#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"

#include <utility>

namespace {
ViewportEnginePresentationTargetAssignmentInput assignmentInput(
    const ViewportEnginePresentationTargetAssignmentRequest& input,
    ViewportEngineGeometryInput geometry)
{
    ViewportEnginePresentationTargetAssignmentInput operationInput { input.presentationTarget,
        input.transitionPolicy, input.primarySource, input.secondarySource, geometry };
    if (!operationInput.presentationTarget.isClear() && !operationInput.primarySource.sequence) {
        operationInput.primarySource = ImageViewportInternal::factorySequenceSource(
            operationInput.presentationTarget.primary());
    }
    if (!operationInput.presentationTarget.isClear() && !operationInput.secondarySource.sequence) {
        operationInput.secondarySource = ImageViewportInternal::factorySequenceSource(
            operationInput.presentationTarget.secondary());
    }
    return operationInput;
}
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

bool ViewportEngine::hasCompleteCommittedPresentation() const
{
    const auto& request = m_state->requestState.request;
    const auto& display = m_state->displayState.display;
    const auto& target = m_state->requestState.presentationTarget;
    if (request.status != ImageViewportRequestStatus::Ready
        || display.status != ImageViewportDisplayStatus::Ready || request.sequenceGeneration == 0
        || target.generation != request.sequenceGeneration || !target.acceptedRoleSet.primary()
        || display.roles[0].displayedRequest.generation != request.sequenceGeneration
        || !display.roles[0].displayedPayload.hasPresentableContent()) {
        return false;
    }
    if (!target.acceptedRoleSet.secondary()) {
        return true;
    }
    return display.roles[1].displayedRequest.generation == request.sequenceGeneration
        && display.roles[1].displayedPayload.hasPresentableContent();
}

ViewportProviderTransportBatch ViewportEngine::pinCurrentPresentationForRestoration()
{
    ViewportProviderTransportBatch transport;
    ViewportEngineRestorationState restoration {
        m_state->requestState.presentationTarget,
        m_state->requestState.request,
        m_state->displayState.display,
        m_state->providerState,
        m_state->playbackState.playback,
        m_state->presentationState.presentation,
        m_state->revisions.targetPresentationRevision,
    };
    for (const auto role : { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        auto& provider = restoration.provider.roles[roleIndex(role)].provider;
        const ImageSequenceProviderRequestToken frameToken = provider.requests.frameToken();
        if (frameToken.isValid()) {
            ViewportProviderTransportCommand command;
            command.kind = ViewportProviderTransportCommand::Kind::SendRequest;
            command.role = role;
            command.request = ImageSequenceProviderRequest::cancel({ frameToken });
            command.reportDispatchFailure = false;
            transport.append(std::move(command));
            provider.requests.retireFrame();
        }
        provider.requests.clearQueue();
        provider.requests.lastIssuedFrameDemand.reset();
    }
    m_state->restoration = std::move(restoration);
    return transport;
}

void ViewportEngine::retireRestoration(ViewportEngineTransitionDraft& transition)
{
    if (!m_state->restoration) {
        return;
    }
    const auto& restoration = *m_state->restoration;
    for (const auto role : { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        const auto& provider = restoration.provider.roles[roleIndex(role)].provider;
        if (!provider.session.sessionActive) {
            continue;
        }
        ViewportProviderTransportCommand command;
        command.kind = ViewportProviderTransportCommand::Kind::CloseSession;
        command.role = role;
        command.sessionClose.metadataToken = provider.requests.metadataToken();
        command.sessionClose.frameToken = provider.requests.frameToken();
        command.generation = restoration.request.sequenceGeneration;
        command.sessionSerial = provider.session.sessionSerial;
        transition.providerTransport.append(std::move(command));
    }
    m_state->restoration.reset();
}

bool ViewportEngine::restorePreviousIfTerminal(ViewportEngineTransitionDraft& transition)
{
    if (!m_state->restoration) {
        return false;
    }
    const ViewportEngineProjectedTerminal projected
        = projectViewportEngineTerminal(m_state->requestState.request);
    if (!projected.terminal) {
        return false;
    }

    const auto& terminal = *projected.terminal;
    ViewportEngineRecoveredTransitionFailure recovered {
        ImageViewportFailureSnapshot(true, ImageViewportFailureContext::RestoredTransition,
            terminal.reason, QVariant::fromValue(projected.role), projected.scope,
            terminal.providerFailureAvailable, terminal.providerCause, terminal.providerReference),
        terminal.diagnostic,
        terminal.providerFailureLeaseId,
    };

    const auto closeFailedSession = [this, &transition](ImageViewportPageRole role) {
        const auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
        const quint64 generation = m_state->requestState.request.sequenceGeneration;
        const quint64 sessionSerial = provider.session.sessionSerial;
        const qsizetype firstCommand = transition.providerTransport.size();
        appendProviderTransport(transition.providerTransport, closeProviderSession(role), role);
        for (auto command = transition.providerTransport.begin() + firstCommand;
            command != transition.providerTransport.end(); ++command) {
            if (command->kind == ViewportProviderTransportCommand::Kind::CloseSession) {
                command->generation = generation;
                command->sessionSerial = sessionSerial;
            }
        }
    };
    closeFailedSession(ImageViewportPageRole::Primary);
    closeFailedSession(ImageViewportPageRole::Secondary);

    ViewportEngineRestorationState restoration = std::move(*m_state->restoration);
    m_state->restoration.reset();
    m_state->requestState.presentationTarget = std::move(restoration.presentationTarget);
    m_state->requestState.request = std::move(restoration.request);
    m_state->displayState.display = std::move(restoration.display);
    m_state->providerState = std::move(restoration.provider);
    m_state->playbackState.playback = restoration.playback;
    m_state->presentationState.presentation = restoration.presentation;
    m_state->revisions.targetPresentationRevision = restoration.targetPresentationRevision;
    m_state->renderCoordination.activeAttempt.reset();
    m_state->recoveredTransitionFailure = std::move(recovered);

    for (const auto role : { ImageViewportPageRole::Primary, ImageViewportPageRole::Secondary }) {
        const auto& provider = m_state->providerState.roles[roleIndex(role)].provider;
        if (!provider.session.sessionActive) {
            continue;
        }
        ViewportProviderTransportCommand command;
        command.kind = ViewportProviderTransportCommand::Kind::ActivateSession;
        command.role = role;
        command.generation = m_state->requestState.request.sequenceGeneration;
        command.sessionSerial = provider.session.sessionSerial;
        transition.providerTransport.append(std::move(command));
    }

    transition.changes.requestState = true;
    transition.changes.displayState = true;
    transition.changes.geometryState = true;
    transition.changes.playbackPhase = true;
    transition.changes.diagnostics = true;
    transition.changes.requestRevision = true;
    transition.changes.displayRevision = true;
    transition.changes.presentationRevision = true;
    transition.changes.targetPresentationRevision = true;
    transition.changes.adoptTargetPresentationRevision = false;
    transition.changes.scheduleUpdate = true;
    transition.playbackSchedules = currentPlaybackSchedules();
    return true;
}

void ViewportEngine::commitReplacementIfReady(ViewportEngineTransitionDraft& transition)
{
    if (!m_state->restoration
        || m_state->requestState.request.status != ImageViewportRequestStatus::Ready
        || m_state->displayState.display.status != ImageViewportDisplayStatus::Ready) {
        return;
    }
    retireRestoration(transition);
}

ViewportEngineTransition ViewportEngine::handleResourcePressure()
{
    ViewportEngineTransitionDraft transition;
    auto& display = m_state->displayState.display;
    if (display.status != ImageViewportDisplayStatus::Retained || m_state->restoration) {
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
    const auto collect = [&leases](const ImageViewportInternal::DisplayState& display) {
        for (const auto& role : display.roles) {
            if (role.displayedPayload.providerFrameLeaseId != 0) {
                leases.insert(role.displayedPayload.providerFrameLeaseId);
            }
            if (role.pendingRenderPayload.providerFrameLeaseId != 0) {
                leases.insert(role.pendingRenderPayload.providerFrameLeaseId);
            }
        }
    };
    collect(m_state->displayState.display);
    if (m_state->restoration) {
        collect(m_state->restoration->display);
    }
    return leases;
}

QSet<quint64> ViewportEngine::providerFailureLeaseIds() const
{
    const ViewportEngineProjectedTerminal projected
        = projectViewportEngineTerminal(m_state->requestState.request);
    if (!projected.terminal || projected.terminal->providerFailureLeaseId == 0) {
        return m_state->recoveredTransitionFailure
                && m_state->recoveredTransitionFailure->providerFailureLeaseId != 0
            ? QSet<quint64> {
                  m_state->recoveredTransitionFailure->providerFailureLeaseId,
              }
            : QSet<quint64> {};
    }
    QSet<quint64> leases { projected.terminal->providerFailureLeaseId };
    if (m_state->recoveredTransitionFailure
        && m_state->recoveredTransitionFailure->providerFailureLeaseId != 0) {
        leases.insert(m_state->recoveredTransitionFailure->providerFailureLeaseId);
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
        m_state->revisions.snapshotRevision, m_state->recoveredTransitionFailure };
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
    auto operationInput = assignmentInput(input, acceptedGeometry());
    const bool restorePrevious = operationInput.transitionPolicy.failureTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::FailureTransition::RestorePrevious;
    if (!validateViewportEnginePresentationTargetAssignment(operationInput,
            m_state->requestState.presentationTarget, m_state->requestState.request,
            m_state->providerState.roles)
        || (restorePrevious && (m_state->restoration || !hasCompleteCommittedPresentation()))) {
        return finalizeCommandTransition(rejectInvalidCommand(), {});
    }
    ViewportEngineTransitionDraft result;
    if (restorePrevious) {
        result.providerTransport.append(pinCurrentPresentationForRestoration());
    } else {
        retireRestoration(result);
    }
    ViewportEnginePresentationTargetAssignmentAccess access(
        { m_state->requestState.presentationTarget,
            m_state->requestState.nextPresentationTargetGeneration, m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation });
    auto reduction = reduceViewportEnginePresentationTargetAssignment(
        std::move(operationInput), std::move(access));
    m_state->requestState.presentationTarget = std::move(reduction.mutation.target);
    m_state->requestState.nextPresentationTargetGeneration
        = reduction.mutation.nextTargetGeneration;
    m_state->requestState.request = std::move(reduction.mutation.request);
    m_state->playbackState.playback = reduction.mutation.playback;
    m_state->displayState.display = std::move(reduction.mutation.display);
    m_state->providerState.roles = std::move(reduction.mutation.roles);
    m_state->presentationState.presentation = reduction.mutation.presentation;
    ViewportEnginePayloadAllocationRebuildResult allocation;
    if (reduction.presentationTargetChanged) {
        allocation = rebuildViewportEnginePayloadAllocation(m_state->requestState.request,
            m_state->displayState.display, m_state->restoration.has_value());
    }
    if (reduction.presentationTargetChanged) {
        m_state->revisions.targetPresentationRevision
            = reduction.clear ? 0 : advanceTargetPresentationRevision();
    }
    const ViewportEngineCommandResult command
        = reduction.presentationTargetChanged ? accepted() : acceptedPreservingCommandDiagnostics();
    result.changes = reduction.changes;
    if (reduction.presentationTargetChanged) {
        m_state->recoveredTransitionFailure.reset();
    }
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
    restorePreviousIfTerminal(result);
    result.playbackSchedules = currentPlaybackSchedules();
    return finalizeCommandTransition(command, std::move(result));
}

bool ViewportEngine::canAssignPresentationTarget(
    const ViewportEnginePresentationTargetAssignmentRequest& input) const
{
    const auto operationInput = assignmentInput(input, acceptedGeometry());
    const bool restorePrevious = operationInput.transitionPolicy.failureTransition()
        == ViewportEnginePresentationTargetTransitionPolicy::FailureTransition::RestorePrevious;
    return (!restorePrevious || (!m_state->restoration && hasCompleteCommittedPresentation()))
        && validateViewportEnginePresentationTargetAssignment(operationInput,
            m_state->requestState.presentationTarget, m_state->requestState.request,
            m_state->providerState.roles);
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
