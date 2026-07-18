// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineplaybackoperations_p.h"
#include "viewportprovidertransporteffects_p.h"

#include "viewportplaybackcontract_p.h"

#include <algorithm>
#include <utility>

namespace {

void mergeChanges(ImageViewportInternal::ViewportChangeSet& target,
    const ImageViewportInternal::ViewportChangeSet& source)
{
    target.requestState |= source.requestState;
    target.displayState |= source.displayState;
    target.playbackPhase |= source.playbackPhase;
    target.diagnostics |= source.diagnostics;
    target.displayRevision |= source.displayRevision;
    target.requestRevision |= source.requestRevision;
    target.commandRevision |= source.commandRevision;
    target.presentationRevision |= source.presentationRevision;
    target.targetPresentationRevision |= source.targetPresentationRevision;
    target.adoptTargetPresentationRevision |= source.adoptTargetPresentationRevision;
    target.scheduleUpdate |= source.scheduleUpdate;
}

const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewportPageRole role)
{
    return request.roles[role == ImageViewportPageRole::Secondary ? 1U : 0U];
}

bool rolePresent(const ImageViewportInternal::RequestState& request, ImageViewportPageRole role)
{
    return requestRole(request, role).source.facts.present;
}

bool hasStateChanges(const ImageViewportInternal::ViewportChangeSet& changes)
{
    return changes.requestState || changes.displayState || changes.geometryState
        || changes.playbackPhase || changes.diagnostics || changes.displayRevision
        || changes.requestRevision || changes.presentationRevision || changes.scheduleUpdate;
}

bool hasProviderEffects(const std::array<ViewportProviderFrameTransportEffect, 2>& effects)
{
    return std::any_of(effects.cbegin(), effects.cend(), [](const auto& effect) {
        return effect.cancelToken.isValid()
            || effect.deferredEngineEvent != ViewportProviderDeferredEngineEvent::None
            || effect.closeSession || effect.sendCommand;
    });
}

void commitPlaybackMutation(
    ViewportEngineCanonicalState& state, ViewportEnginePlaybackMutation mutation)
{
    state.requestState.request = std::move(mutation.request);
    state.playbackState.playback = std::move(mutation.playback);
    state.displayState.display = std::move(mutation.display);
    state.providerState.roles = std::move(mutation.roles);
    state.revisions.nextRevision = mutation.nextRevision;
}
}

ViewportEngineCommandTransition ViewportEngine::applyPlaybackCommand(
    ViewportEnginePlaybackCommandRequest input)
{
    ViewportEngineTransitionDraft transition;
    const auto appendEffects
        = [&transition](const std::array<ViewportProviderFrameTransportEffect, 2>& effects) {
              appendProviderTransport(
                  transition.providerTransport, effects[0], ImageViewportPageRole::Primary);
              appendProviderTransport(
                  transition.providerTransport, effects[1], ImageViewportPageRole::Secondary);
          };
    const GeometryInput geometry = acceptedGeometry();
    if (!validateViewportPlaybackCommand(input.command)) {
        transition.playbackSchedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return finalizeCommandTransition(rejectInvalidCommand(), std::move(transition));
    }
    if (!rolePresent(m_state->requestState.request, input.command.role)) {
        transition.playbackSchedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return finalizeCommandTransition(rejected(ImageViewportCommandOutcome::IgnoredNoRequest,
                                             ImageViewportCommandReason::IgnoredNoRequest),
            std::move(transition));
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Pause) {
        ViewportEnginePlaybackPauseAccess access(m_state->playbackState.playback);
        const auto reduction = reduceViewportEnginePlaybackPause({ input.command.role }, access);
        m_state->playbackState.playback = std::move(access.takeMutation().playback);
        const auto command
            = reduction.playbackPhaseChanged ? accepted() : acceptedPreservingCommandDiagnostics();
        transition.changes.playbackPhase = reduction.playbackPhaseChanged;
        transition.playbackSchedule = currentPlaybackSchedule();
        return finalizeCommandTransition(command, std::move(transition));
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Stop) {
        ViewportEnginePlaybackStopAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackStop({ input.command.role, geometry }, access);
        commitPlaybackMutation(*m_state, access.takeMutation());
        const auto command = hasStateChanges(reduction.changes)
                || hasProviderEffects(reduction.providerFrameTransport)
            ? accepted()
            : acceptedPreservingCommandDiagnostics();
        mergeChanges(transition.changes, reduction.changes);
        appendEffects(reduction.providerFrameTransport);
        transition.playbackSchedule = currentPlaybackSchedule();
        return finalizeCommandTransition(command, std::move(transition));
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::SeekFrame
        || input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition) {
        ViewportEnginePlaybackSeekAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackSeek(
            { input.command.kind, input.command.role, input.command.value, geometry }, access);
        if (reduction.outcome == ImageViewportCommandOutcome::Accepted) {
            commitPlaybackMutation(*m_state, access.takeMutation());
        }
        const bool changed = hasStateChanges(reduction.changes)
            || hasProviderEffects(reduction.providerFrameTransport);
        const auto command = reduction.outcome != ImageViewportCommandOutcome::Accepted
            ? rejected(reduction.outcome, reduction.reason)
            : changed ? accepted()
                      : acceptedPreservingCommandDiagnostics();
        mergeChanges(transition.changes, reduction.changes);
        appendEffects(reduction.providerFrameTransport);
        transition.playbackSchedule = reduction.outcome == ImageViewportCommandOutcome::Accepted
            ? currentPlaybackSchedule()
            : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange,
                  -1 };
        return finalizeCommandTransition(command, std::move(transition));
    }
    ViewportEnginePlaybackPlayAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto reduction = reduceViewportEnginePlaybackPlay({ input.command.role, geometry }, access);
    if (reduction.outcome == ImageViewportCommandOutcome::Accepted) {
        commitPlaybackMutation(*m_state, access.takeMutation());
        m_state->playbackState.playback.authoredAutoplayArbitration
            = ImageViewportInternal::AuthoredAutoplayArbitrationState::Suppressed;
    }
    const bool changed = hasStateChanges(reduction.changes)
        || hasProviderEffects(reduction.providerFrameTransport);
    const auto command = reduction.outcome != ImageViewportCommandOutcome::Accepted
        ? rejected(reduction.outcome, reduction.reason)
        : changed ? accepted()
                  : acceptedPreservingCommandDiagnostics();
    mergeChanges(transition.changes, reduction.changes);
    appendEffects(reduction.providerFrameTransport);
    transition.playbackSchedule = reduction.outcome == ImageViewportCommandOutcome::Accepted
        ? currentPlaybackSchedule()
        : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
    return finalizeCommandTransition(command, std::move(transition));
}

ViewportEngineTransition ViewportEngine::advancePlayback(ViewportEnginePlaybackTickRequest input)
{
    const GeometryInput geometry = acceptedGeometry();
    ViewportEnginePlaybackTickAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto reduction
        = reduceViewportEnginePlaybackTick({ input.elapsedMilliseconds, geometry }, access);
    commitPlaybackMutation(*m_state, access.takeMutation());
    ViewportEngineTransitionDraft transition;
    transition.changes = reduction.changes;
    appendProviderTransport(transition.providerTransport, reduction.providerFrameTransport[0],
        ImageViewportPageRole::Primary);
    appendProviderTransport(transition.providerTransport, reduction.providerFrameTransport[1],
        ImageViewportPageRole::Secondary);
    transition.playbackSchedule = reduction.projectSchedule
        ? currentPlaybackSchedule()
        : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
    return finalizeTransition(std::move(transition));
}

ViewportPlaybackScheduleEffect ViewportEngine::currentPlaybackSchedule() const
{
    ViewportEnginePlaybackScheduleAccess access(
        m_state->requestState.request, m_state->playbackState.playback, providerFactsView());
    return projectViewportPlaybackSchedule(std::move(access));
}
