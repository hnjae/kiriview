#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"
#include "viewportengineplaybackoperations_p.h"

#include "imageviewporttoken_p.h"
#include "viewportplaybackcontract_p.h"

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
    target.scheduleUpdate |= source.scheduleUpdate;
}

const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}

bool rolePresent(const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
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
        result.command = rejected(ImageViewport::CommandOutcome::IgnoredNoRequest,
            ImageViewport::CommandReason::IgnoredNoRequest);
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Pause) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        ViewportEnginePlaybackPauseAccess access(m_state->playbackState.playback);
        const auto reduction
            = reduceViewportEnginePlaybackPause({ input.command.role }, std::move(access));
        result.changes.playbackPhase = reduction.playbackPhaseChanged;
        result.schedule = currentPlaybackSchedule();
        return result;
    }
    if (input.command.kind == ViewportPlaybackCommand::Kind::Stop) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        ViewportEnginePlaybackStopAccess access(m_state->requestState.request,
            m_state->playbackState.playback, m_state->displayState.display,
            m_state->providerState.roles, m_state->presentationState.presentation,
            m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackStop(
            { input.command.role, geometry }, std::move(access));
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
            m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
            m_state->requestState.presentationTarget.generation);
        auto reduction = reduceViewportEnginePlaybackSeek(
            { input.command.kind, input.command.role, input.command.value, geometry },
            std::move(access));
        result.command = reduction.outcome == ImageViewport::CommandOutcome::Accepted
            ? accepted()
            : rejected(reduction.outcome, reduction.reason);
        appendCommandChanges(result.command, result.changes);
        mergeChanges(result.changes, reduction.changes);
        result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
        result.schedule = reduction.outcome == ImageViewport::CommandOutcome::Accepted
            ? currentPlaybackSchedule()
            : ViewportPlaybackScheduleEffect {
                  ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    ViewportEnginePlaybackPlayAccess access(m_state->requestState.request,
        m_state->playbackState.playback, m_state->displayState.display,
        m_state->providerState.roles, m_state->presentationState.presentation,
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
        m_state->requestState.presentationTarget.generation);
    auto reduction = reduceViewportEnginePlaybackPlay(
        { input.command.role, geometry }, std::move(access));
    result.command = reduction.outcome == ImageViewport::CommandOutcome::Accepted
        ? accepted()
        : rejected(reduction.outcome, reduction.reason);
    appendCommandChanges(result.command, result.changes);
    mergeChanges(result.changes, reduction.changes);
    result.effects.providerFrameTransport = std::move(reduction.providerFrameTransport);
    result.schedule = reduction.outcome == ImageViewport::CommandOutcome::Accepted
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
        m_state->revisions.nextRevision, m_state->revisions.presentationRevision,
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
