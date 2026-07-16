#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineplaybackoperations_p.h"

#include "imageviewporttoken_p.h"
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

void appendCommandChanges(
    const ViewportEngineCommandResult& command, ImageViewportInternal::ViewportChangeSet& changes)
{
    if (!command.commandRevisionChanged) {
        return;
    }
    changes.commandRevision = true;
    changes.commandRevisionValue
        = ImageViewportInternal::RevisionTokenPrivateAccess::value(command.commandRevision);
    changes.diagnostics = true;
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
}

ViewportEnginePlaybackCommandResult ViewportEngine::applyPlaybackCommand(
    const ViewportEnginePlaybackCommandRequest& input)
{
    ViewportEnginePlaybackCommandResult result;
    const GeometryInput geometry = acceptedGeometry(input.viewport);
    if (!validateViewportPlaybackCommand(input.command)) {
        result.command = rejectInvalidCommand();
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    if (!rolePresent(m_state->requestState.request, input.command.role)) {
        result.command = rejected(ImageViewportCommandOutcome::IgnoredNoRequest,
            ImageViewportCommandReason::IgnoredNoRequest);
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Pause) {
        ViewportEnginePlaybackPauseAccess access(m_state->playbackState.playback);
        const auto reduction
            = reduceViewportEnginePlaybackPause({ input.command.role }, std::move(access));
        result.command
            = reduction.playbackPhaseChanged ? accepted() : acceptedPreservingCommandDiagnostics();
        appendCommandChanges(result.command, result.changes);
        result.changes.playbackPhase = reduction.playbackPhaseChanged;
        result.schedule = currentPlaybackSchedule();
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Stop) {
        ViewportEnginePlaybackStopAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction
            = reduceViewportEnginePlaybackStop({ input.command.role, geometry }, std::move(access));
        result.command = hasStateChanges(reduction.changes)
                || hasProviderEffects(reduction.providerFrameTransport)
            ? accepted()
            : acceptedPreservingCommandDiagnostics();
        appendCommandChanges(result.command, result.changes);
        mergeChanges(result.changes, reduction.changes);
        result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
        result.schedule = currentPlaybackSchedule();
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::SeekFrame
        || input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition) {
        ViewportEnginePlaybackSeekAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackSeek(
            { input.command.kind, input.command.role, input.command.value, geometry },
            std::move(access));
        const bool changed = hasStateChanges(reduction.changes)
            || hasProviderEffects(reduction.providerFrameTransport);
        result.command = reduction.outcome != ImageViewportCommandOutcome::Accepted
            ? rejected(reduction.outcome, reduction.reason)
            : changed ? accepted()
                      : acceptedPreservingCommandDiagnostics();
        appendCommandChanges(result.command, result.changes);
        mergeChanges(result.changes, reduction.changes);
        result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
        result.schedule = reduction.outcome == ImageViewportCommandOutcome::Accepted
            ? currentPlaybackSchedule()
            : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange,
                  -1 };
        return result;
    }
    ViewportEnginePlaybackPlayAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto reduction
        = reduceViewportEnginePlaybackPlay({ input.command.role, geometry }, std::move(access));
    const bool changed = hasStateChanges(reduction.changes)
        || hasProviderEffects(reduction.providerFrameTransport);
    result.command = reduction.outcome != ImageViewportCommandOutcome::Accepted
        ? rejected(reduction.outcome, reduction.reason)
        : changed ? accepted()
                  : acceptedPreservingCommandDiagnostics();
    appendCommandChanges(result.command, result.changes);
    mergeChanges(result.changes, reduction.changes);
    result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
    result.schedule = reduction.outcome == ImageViewportCommandOutcome::Accepted
        ? currentPlaybackSchedule()
        : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
    return result;
}

ViewportEnginePlaybackTickResult ViewportEngine::advancePlayback(
    const ViewportEnginePlaybackTickRequest& input)
{
    ViewportEnginePlaybackTickResult result;
    const GeometryInput geometry = acceptedGeometry(input.viewport);
    ViewportEnginePlaybackTickAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.targetPresentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto reduction = reduceViewportEnginePlaybackTick(
        { input.elapsedMilliseconds, geometry }, std::move(access));
    result.changes = reduction.changes;
    result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
    result.schedule = reduction.projectSchedule
        ? currentPlaybackSchedule()
        : ViewportPlaybackScheduleEffect { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
    return result;
}

ViewportPlaybackScheduleEffect ViewportEngine::currentPlaybackSchedule() const
{
    ViewportEnginePlaybackScheduleAccess access(
        m_state->requestState.request, m_state->playbackState.playback, providerFactsView());
    return projectViewportPlaybackSchedule(std::move(access));
}
