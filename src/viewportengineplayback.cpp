#include "viewportengine_p.h"
#include "viewportenginecapabilities_p.h"

#include "viewportplaybackcontract_p.h"
#include "playbacktimeline_p.h"
#include "imageviewporttoken_p.h"
#include "imageviewportvalidation_p.h"
#include "imageviewportproviderfacts_p.h"

#include <algorithm>
#include <limits>

namespace {

const ImageViewportInternal::RequestState::RoleState& requestRole(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}

ImageViewportInternal::RequestState::RoleState& requestRole(
    ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return request.roles[role == ImageViewport::PageRole::Secondary ? 1U : 0U];
}

bool rolePresent(
    const ImageViewportInternal::RequestState& request, ImageViewport::PageRole role)
{
    return requestRole(request, role).source.facts.present;
}

void appendCommandChanges(const ViewportEngine::CommandResult& command,
    ImageViewportInternal::ViewportChangeSet& changes)
{
    if (!command.commandRevisionChanged) {
        return;
    }
    changes.commandRevision = true;
    changes.commandRevisionValue
        = ImageViewportInternal::RevisionTokenPrivateAccess::value(command.commandRevision);
    changes.diagnostics = true;
}

bool effectiveLooping(const ImageViewportInternal::PlaybackState& playback,
    ImageSequenceAuthoredAnimationFacts facts)
{
    if (playback.looping) {
        return true;
    }
    switch (facts.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return playback.loopIterationsCompleted + 1 < facts.loopCount();
    case ImageSequenceAuthoredAnimationFacts::LoopMode::PlayOnce:
        return false;
    }
    return false;
}

}

ViewportEngine::PlaybackCommandResult ViewportEngine::applyPlaybackCommand(
    const PlaybackCommandInput& input)
{
    PlaybackCommandResult result;
    if (!ImageViewportInternal::isValidPageRole(input.command.role)) {
        result.command = rejectInvalidCommand();
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    if (!rolePresent(playbackAccess().request(), input.command.role)) {
        result.command = rejected(ImageViewport::CommandOutcome::IgnoredNoRequest,
            ImageViewport::CommandReason::IgnoredNoRequest);
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    const auto& terminal = playbackAccess().request().targetSpreadTerminal;
    const auto generationTerminal = terminal.sealed
        && terminal.generation == playbackAccess().request().sequenceGeneration
        && terminal.requestId == playbackAccess().request().roles[0].activeRequest.identity.id
        && ((terminal.primary.terminal
                && terminal.primary.failureScope
                    == ImageViewportInternal::FailureScope::Generation)
            || (terminal.secondary.terminal
                && terminal.secondary.failureScope
                    == ImageViewportInternal::FailureScope::Generation));
    const auto& commandSource = requestRole(playbackAccess().request(), input.command.role).source;
    const auto& commandProvider = playbackAccess().roles()[roleIndex(input.command.role)].provider;
    const bool providerTransportUnavailableTerminal = commandSource.facts.provider
        && !commandProvider.sessionActive
        && (playbackAccess().request().status == ImageViewport::RequestStatus::Unsupported
            || playbackAccess().request().status == ImageViewport::RequestStatus::Error);
    if ((generationTerminal || providerTransportUnavailableTerminal)
        && (input.command.kind == ViewportPlaybackCommand::Kind::Play
            || ((input.command.kind == ViewportPlaybackCommand::Kind::SeekFrame
                    || input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition)
                && input.command.value >= 0))) {
        result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
            ImageViewport::CommandReason::UnsupportedRequest);
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }

    auto beginRoleRequest = [this](ImageViewport::PageRole role,
                                ImageViewportInternal::DisplayRequestOrigin origin,
                                ImageViewportInternal::DisplayRequestTarget target,
                                ImageViewportInternal::ResolvedFrameIdentity resolved,
                                bool remember) {
        if (role == ImageViewport::PageRole::Primary) {
            playbackAccess().request().beginDisplayRequest(origin, target, resolved, remember);
            return;
        }
        const auto primaryRequest = playbackAccess().request().roles[0].activeRequest;
        playbackAccess().request().beginDisplayRequest(
            origin, primaryRequest.target, primaryRequest.resolvedFrame, false);
        auto& secondary = playbackAccess().request().roles[1].activeRequest;
        secondary.identity = playbackAccess().request().roles[0].activeRequest.identity;
        secondary.target = target;
        secondary.resolvedFrame = resolved;
        secondary.providerFrameToken = {};
        secondary.preparedPayloadId = 0;
        if (remember) {
            playbackAccess().request().roles[1].latestNonPlaybackRequest = secondary;
        }
    };
    auto stageBuiltIn = [this, &input]() {
        playbackAccess().request().targetSpreadTerminal.clear();
        playbackAccess().request().lastAcceptedRenderFailure = {};
        playbackAccess().display().captureRenderFailureRetainedDisplay(true);
        playbackAccess().display().roles[0].pendingRenderPayload.commitPending = true;
        playbackAccess().display().beginPreparedPayloadIdentity(
            playbackAccess().request().sequenceGeneration, playbackAccess().request().roles[0].activeRequest);
        playbackAccess().display().roles[0].pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(playbackAccess().request().roles[0].source,
                  playbackAccess().request().roles[0].activeRequest.target.frame,
                  playbackAccess().display().roles[0].pendingRenderPayload)
                  .preparedPayload;
        if (playbackAccess().request().roles[1].source.facts.present
            && !playbackAccess().request().roles[1].source.facts.provider
            && playbackAccess().request().roles[1].activeRequest.target.frame >= 0) {
            ImageViewportInternal::PreparedPayload payload;
            payload.commitPending = true;
            payload.generation = playbackAccess().request().sequenceGeneration;
            payload.requestId = playbackAccess().request().roles[0].activeRequest.identity.id;
            payload.payloadId = ++playbackAccess().display().nextPreparedPayloadId;
            playbackAccess().request().roles[1].activeRequest.preparedPayloadId = payload.payloadId;
            playbackAccess().display().roles[1].pendingRenderPayload
                = FramePreparation::admitBuiltInFrame(
                      playbackAccess().request().roles[1].source,
                      playbackAccess().request().roles[1].activeRequest.target.frame, payload)
                      .preparedPayload;
        }
        playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
        playbackAccess().request().reason = input.geometry.itemBounds.isEmpty()
            ? ImageViewport::RequestReason::RenderWaiting
            : ImageViewport::RequestReason::UploadPending;
        playbackAccess().display().status = playbackAccess().display().roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
    };
    auto markRequest = [&result]() {
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.scheduleUpdate = true;
    };
    auto dispatchProvider = [this, &result, &markRequest, &input](ImageViewport::PageRole role,
                                ImageViewportInternal::DisplayRequestTarget target) {
        const std::size_t index = role == ImageViewport::PageRole::Secondary ? 1U : 0U;
        auto& effect = result.effects.providerFrameTransport[index];
        auto& provider = playbackAccess().roles()[roleIndex(role)].provider;
        auto& active = requestRole(playbackAccess().request(), role).activeRequest;
        if (provider.activeFrameToken.isValid()) {
            const auto queued = queueProviderFrameRequest({ role, target.frame,
                target.providerTargetKind });
            effect.cancelToken = queued.cancelToken;
            effect.deferredControllerEvent = queued.deferredFlush
                ? ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest
                : ViewportProviderDeferredControllerEvent::None;
            return true;
        }
        if (provider.nextRequestToken == std::numeric_limits<quint64>::max()) {
            effect.closeSession = provider.sessionActive;
            effect.sessionClose.metadataToken = provider.activeMetadataToken;
            effect.sessionClose.frameToken = provider.activeFrameToken;
            provider.activeMetadataToken = {};
            provider.activeFrameToken = {};
            provider.nextRequestToken = 0;
            playbackAccess().request().status = ImageViewport::RequestStatus::Error;
            playbackAccess().request().reason = ImageViewport::RequestReason::ProviderFailure;
            playbackAccess().request().errorString = QStringLiteral("provider request token exhausted");
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Stopped;
            playbackAccess().display().status = playbackAccess().display().status == ImageViewport::DisplayStatus::Empty
                ? ImageViewport::DisplayStatus::Empty
                : ImageViewport::DisplayStatus::Retained;
            result.changes.playbackPhase = true;
            result.changes.diagnostics = true;
            markRequest();
            return false;
        }
        provider.activeFrameToken
            = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(
                ++provider.nextRequestToken);
        active.providerFrameToken = provider.activeFrameToken;
        effect.sendCommand = provider.sessionActive;
        effect.command.token = provider.activeFrameToken;
        effect.command.frame = active.resolvedFrame.frame;
        effect.command.position = active.target.position;
        effect.command.targetKind = active.target.providerTargetKind;
        effect.command.demand = providerDisplayDemand(role, input.geometry);
        playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
        playbackAccess().request().reason = ImageViewport::RequestReason::ProviderWaiting;
        playbackAccess().display().status = playbackAccess().display().roles[0].displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
        playbackAccess().display().clearPendingRenderPayload();
        playbackAccess().display().clearRenderFailureRetainedDisplay();
        return true;
    };

    if (input.command.kind == ViewportPlaybackCommand::Kind::Pause
        && (playbackAccess().playback().phase == ImageViewport::PlaybackPhase::Stopped
            || playbackAccess().playback().role == input.command.role)
        && (playbackAccess().playback().phase == ImageViewport::PlaybackPhase::Playing
            || playbackAccess().playback().phase == ImageViewport::PlaybackPhase::Waiting)) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    } else if (input.command.kind == ViewportPlaybackCommand::Kind::Pause) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
    } else if (input.command.kind == ViewportPlaybackCommand::Kind::Play) {
        const auto& source = requestRole(playbackAccess().request(), input.command.role).source;
        auto& provider = playbackAccess().roles()[roleIndex(input.command.role)].provider;
        if (source.facts.provider && !provider.metadataReady) {
            if (ImageViewportInternal::providerCapabilityKnownFalse(
                    source.facts.providerTimedPlaybackCapability)) {
                result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                    ImageViewport::CommandReason::UnsupportedRequest);
                appendCommandChanges(result.command, result.changes);
                result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
                return result;
            }
            result.command = accepted();
            appendCommandChanges(result.command, result.changes);
            playbackAccess().playback().role = input.command.role;
            playbackAccess().playback().stopWhenRequestReady = false;
            playbackAccess().playback().loopIterationsCompleted = 0;
            const std::size_t index
                = input.command.role == ImageViewport::PageRole::Secondary ? 1U : 0U;
            if (provider.activeFrameToken.isValid()) {
                result.effects.providerFrameTransport[index].cancelToken
                    = provider.activeFrameToken;
                provider.activeFrameToken = {};
            }
            if (input.command.role == ImageViewport::PageRole::Primary) {
                beginRoleRequest(input.command.role,
                    ImageViewportInternal::DisplayRequestOrigin::Playback,
                    { -1, -1, ImageViewportInternal::ProviderRequestTargetKind::Playback },
                    { -1, -1 }, false);
            } else {
                auto& secondary = playbackAccess().request().roles[1].activeRequest;
                secondary.target = { -1, -1,
                    ImageViewportInternal::ProviderRequestTargetKind::Playback };
                secondary.resolvedFrame = { -1, -1 };
                secondary.providerFrameToken = {};
            }
            playbackAccess().playback().providerStartPending
                = input.command.role == ImageViewport::PageRole::Primary;
            playbackAccess().playback().position = -1;
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
            result.changes.playbackPhase = true;
            markRequest();
            result.schedule = playbackScheduleEffect();
            return result;
        }
        if (source.facts.provider
            && (!provider.timedMetadata || !provider.timedPlaybackSupport)) {
            result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
            appendCommandChanges(result.command, result.changes);
            result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
            return result;
        }
        if (!source.facts.provider
            && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
            result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
            appendCommandChanges(result.command, result.changes);
            result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
            return result;
        }
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        if (!source.facts.provider
            && (playbackAccess().request().status == ImageViewport::RequestStatus::Unsupported
                || playbackAccess().request().status == ImageViewport::RequestStatus::Error)) {
            result.changes.diagnostics = playbackAccess().request().clearDiagnostics();
            stageBuiltIn();
            markRequest();
        }
        const bool preservePosition
            = playbackAccess().playback().role == input.command.role
            && playbackAccess().playback().position >= 0
            && !playbackAccess().playback().stopWhenRequestReady
            && playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Stopped;
        playbackAccess().playback().role = input.command.role;
        playbackAccess().playback().stopWhenRequestReady = false;
        if (!preservePosition) {
            const int frame
                = requestRole(playbackAccess().request(), input.command.role).activeRequest.target.frame;
            const auto& intervals
                = source.facts.provider ? provider.timingIntervals : source.facts.timingIntervals;
            playbackAccess().playback().position = intervals.frameStartPosition(frame);
            playbackAccess().playback().loopIterationsCompleted = 0;
        }
        if (source.facts.provider
            && (playbackAccess().request().status == ImageViewport::RequestStatus::Unsupported
                || playbackAccess().request().status == ImageViewport::RequestStatus::Error)) {
            auto& active = requestRole(playbackAccess().request(), input.command.role).activeRequest;
            int frame = active.target.frame;
            if (frame < 0 || frame >= provider.timingIntervals.frameCount()) {
                frame = 0;
            }
            active.target = { frame, provider.timingIntervals.frameStartPosition(frame),
                ImageViewportInternal::ProviderRequestTargetKind::Playback };
            active.resolvedFrame = { frame, provider.timingIntervals.frameStartPosition(frame) };
            playbackAccess().request().clearDiagnostics();
            playbackAccess().request().targetSpreadTerminal.clear();
            dispatchProvider(input.command.role, active.target);
            markRequest();
        }
        const auto phase = playbackAccess().request().status == ImageViewport::RequestStatus::Loading
            ? ImageViewport::PlaybackPhase::Waiting
            : ImageViewport::PlaybackPhase::Playing;
        if (playbackAccess().playback().phase != phase) {
            playbackAccess().playback().phase = phase;
            result.changes.playbackPhase = true;
        }
    } else if (input.command.kind == ViewportPlaybackCommand::Kind::Stop) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        if (playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Stopped
            && playbackAccess().playback().role != input.command.role) {
            result.schedule = playbackScheduleEffect();
            return result;
        }
        playbackAccess().playback().stopWhenRequestReady = false;
        auto& roleState = requestRole(playbackAccess().request(), input.command.role);
        const bool providerSource = roleState.source.facts.provider;
        auto& stopProvider = playbackAccess().roles()[roleIndex(input.command.role)].provider;
        if (providerSource && stopProvider.activeFrameToken.isValid()
            && playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Stopped
            && playbackAccess().playback().role == input.command.role) {
            const std::size_t index
                = input.command.role == ImageViewport::PageRole::Secondary ? 1U : 0U;
            result.effects.providerFrameTransport[index].cancelToken
                = stopProvider.activeFrameToken;
            stopProvider.activeFrameToken = {};
            roleState.activeRequest.providerFrameToken = {};
        }
        auto restore = roleState.latestNonPlaybackRequest;
        if (providerSource && restore.identity.id == 0 && stopProvider.metadataReady) {
            restore = roleState.activeRequest;
            restore.identity.origin = ImageViewportInternal::DisplayRequestOrigin::Initial;
            restore.target.providerTargetKind
                = ImageViewportInternal::ProviderRequestTargetKind::Frame;
        }
        if (providerSource && restore.identity.id != 0 && restore.target.frame < 0
            && stopProvider.metadataReady && stopProvider.timedMetadata) {
            restore.target.frame = 0;
            restore.target.position = stopProvider.timingIntervals.frameStartPosition(0);
            restore.target.providerTargetKind
                = ImageViewportInternal::ProviderRequestTargetKind::Frame;
            restore.resolvedFrame = { 0, stopProvider.timingIntervals.frameStartPosition(0) };
        }
        if (providerSource && restore.target.frame >= 0 && !restore.resolvedFrame.isValid()
            && stopProvider.metadataReady && stopProvider.timedMetadata) {
            const int position
                = stopProvider.timingIntervals.frameStartPosition(restore.target.frame);
            restore.resolvedFrame = { restore.target.frame, position };
            if (restore.target.providerTargetKind
                != ImageViewportInternal::ProviderRequestTargetKind::Position) {
                restore.target.position = position;
            }
        }
        if (playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Stopped
            && restore.identity.id != 0
            && (roleState.activeRequest.identity.origin
                    == ImageViewportInternal::DisplayRequestOrigin::Playback
                || roleState.activeRequest.target.providerTargetKind
                    == ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
            beginRoleRequest(input.command.role,
                ImageViewportInternal::DisplayRequestOrigin::StopRestore, restore.target,
                restore.resolvedFrame, true);
            playbackAccess().playback().position = restore.target.position;
            const auto& displayed = input.command.role == ImageViewport::PageRole::Primary
                ? playbackAccess().display().roles[0].displayedRequest
                : playbackAccess().display().roles[1].displayedRequest;
            const QSizeF displayedSize = input.command.role == ImageViewport::PageRole::Primary
                ? playbackAccess().display().roles[0].displayedImageSize
                : playbackAccess().display().roles[1].displayedImageSize;
            if (displayed.generation == playbackAccess().request().sequenceGeneration
                && displayed.request.resolvedFrame.frame == restore.resolvedFrame.frame
                && displayed.request.resolvedFrame.position == restore.resolvedFrame.position
                && displayedSize.isValid()
                && (!providerSource || restore.resolvedFrame.isValid())) {
                playbackAccess().request().status = ImageViewport::RequestStatus::Ready;
                playbackAccess().request().reason = ImageViewport::RequestReason::Ready;
                playbackAccess().display().status = ImageViewport::DisplayStatus::Ready;
                result.changes.requestState = true;
                result.changes.requestRevision = true;
                result.changes.displayState = true;
                result.changes.displayRevision = true;
            } else {
                if (providerSource && restore.target.frame >= 0
                    && stopProvider.metadataReady) {
                    dispatchProvider(input.command.role, restore.target);
                } else if (providerSource) {
                    playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
                    playbackAccess().request().reason = ImageViewport::RequestReason::ProviderWaiting;
                    playbackAccess().display().status = playbackAccess().display().roles[0].displayedImageSize.isValid()
                        ? ImageViewport::DisplayStatus::Retained
                        : ImageViewport::DisplayStatus::Empty;
                } else {
                    stageBuiltIn();
                }
                markRequest();
            }
        }
        if (playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Stopped) {
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        }
    } else {
        const auto& source = requestRole(playbackAccess().request(), input.command.role).source;
        auto& provider = playbackAccess().roles()[roleIndex(input.command.role)].provider;
        if (source.facts.provider && !provider.metadataReady) {
            const auto capability
                = input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition
                ? source.facts.providerPositionSeekCapability
                : source.facts.providerFrameSeekCapability;
            if (input.command.value < 0) {
                result.command = rejected(ImageViewport::CommandOutcome::Invalid,
                    ImageViewport::CommandReason::InvalidRequest);
            } else if (ImageViewportInternal::providerCapabilityKnownFalse(capability)) {
                result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                    ImageViewport::CommandReason::UnsupportedRequest);
            } else {
                result.command = accepted();
                const auto kind
                    = input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition
                    ? ImageViewportInternal::ProviderRequestTargetKind::Position
                    : ImageViewportInternal::ProviderRequestTargetKind::Frame;
                beginRoleRequest(input.command.role,
                    ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
                    { input.command.kind == ViewportPlaybackCommand::Kind::SeekFrame
                            ? input.command.value
                            : -1,
                        input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition
                            ? input.command.value
                            : -1,
                        kind },
                    { -1, -1 }, true);
                markRequest();
                if (playbackAccess().playback().phase == ImageViewport::PlaybackPhase::Playing) {
                    playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
                    result.changes.playbackPhase = true;
                }
            }
            appendCommandChanges(result.command, result.changes);
            result.schedule = result.command.outcome == ImageViewport::CommandOutcome::Accepted
                ? playbackScheduleEffect()
                : ViewportPlaybackScheduleEffect {
                    ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
            return result;
        }
        if (source.facts.provider) {
            const bool supported
                = input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition
                ? provider.positionSeekSupport
                : provider.frameSeekSupport;
            if (!supported) {
                result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                    ImageViewport::CommandReason::UnsupportedRequest);
                appendCommandChanges(result.command, result.changes);
                result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
                return result;
            }
            const int providerFrameCount
                = provider.timedMetadata ? provider.timingIntervals.frameCount() : 1;
            int frame = input.command.value;
            int position = -1;
            if (input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition) {
                frame = !provider.timedMetadata || input.command.value < 0
                    ? -1
                    : provider.timingIntervals.frameIndexForPosition(input.command.value);
                position = input.command.value;
            } else if (provider.timedMetadata && frame >= 0 && frame < providerFrameCount) {
                position = provider.timingIntervals.frameStartPosition(frame);
            }
            if (frame < 0 || frame >= providerFrameCount) {
                result.command = rejected(ImageViewport::CommandOutcome::Invalid,
                    ImageViewport::CommandReason::InvalidRequest);
                appendCommandChanges(result.command, result.changes);
                result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
                return result;
            }
            result.command = accepted();
            appendCommandChanges(result.command, result.changes);
            const auto kind = input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition
                ? ImageViewportInternal::ProviderRequestTargetKind::Position
                : ImageViewportInternal::ProviderRequestTargetKind::Frame;
            beginRoleRequest(input.command.role,
                ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
                { frame, position, kind },
                { frame,
                    provider.timedMetadata
                        ? provider.timingIntervals.frameStartPosition(frame)
                        : -1 },
                true);
            result.changes.diagnostics = playbackAccess().request().clearDiagnostics();
            dispatchProvider(input.command.role,
                requestRole(playbackAccess().request(), input.command.role).activeRequest.target);
            markRequest();
            if (playbackAccess().playback().phase == ImageViewport::PlaybackPhase::Playing) {
                playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
                result.changes.playbackPhase = true;
            }
            result.schedule = playbackScheduleEffect();
            return result;
        }
        const bool frameSeekOnStill
            = input.command.kind == ViewportPlaybackCommand::Kind::SeekFrame
            && !source.facts.timed && source.facts.frameCount == 1;
        if (!frameSeekOnStill
            && (!source.facts.timed || !source.facts.timingIntervals.isValid())) {
            result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
            appendCommandChanges(result.command, result.changes);
            result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
            return result;
        }
        int frame = input.command.value;
        int position = -1;
        if (input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition) {
            if (input.command.value < 0) {
                frame = -1;
            } else {
                frame = source.facts.timingIntervals.frameIndexForPosition(input.command.value);
                position = input.command.value;
            }
        } else if (frame >= 0 && frame < source.facts.frameCount && source.facts.timed) {
            position = source.facts.timingIntervals.frameStartPosition(frame);
        }
        if (frame < 0 || frame >= source.facts.frameCount) {
            result.command = rejected(ImageViewport::CommandOutcome::Invalid,
                ImageViewport::CommandReason::InvalidRequest);
            appendCommandChanges(result.command, result.changes);
            result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
            return result;
        }
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        const auto targetKind = input.command.kind == ViewportPlaybackCommand::Kind::SeekPosition
            ? ImageViewportInternal::ProviderRequestTargetKind::Position
            : ImageViewportInternal::ProviderRequestTargetKind::Frame;
        beginRoleRequest(input.command.role,
            ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
            { frame, position, targetKind },
            { frame,
                source.facts.timed ? source.facts.timingIntervals.frameStartPosition(frame) : -1 },
            true);
        result.changes.diagnostics = playbackAccess().request().clearDiagnostics();
        stageBuiltIn();
        markRequest();
        if (playbackAccess().playback().phase == ImageViewport::PlaybackPhase::Playing) {
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
            result.changes.playbackPhase = true;
        }
    }
    result.schedule = playbackScheduleEffect();
    return result;
}

void ViewportEngine::setPlaybackPhase(ImageViewport::PlaybackPhase phase,
    ImageViewportInternal::ViewportChangeSet& changes)
{
    if (playbackAccess().playback().phase == phase) {
        return;
    }
    playbackAccess().playback().phase = phase;
    changes.playbackPhase = true;
}

void ViewportEngine::armAuthoredAutoplayIfEligible()
{
    const auto& source = playbackAccess().request().roles[0].source;
    const auto& provider = playbackAccess().roles()[0].provider;
    const auto facts
        = source.facts.provider ? provider.authoredAnimationFacts : source.facts.authoredAnimationFacts;
    if (!facts.autoplay()) {
        return;
    }
    if (source.facts.provider) {
        if (ImageViewportInternal::providerCapabilityKnownFalse(
                source.facts.providerTimedPlaybackCapability)) {
            return;
        }
        playbackAccess().playback().role = ImageViewport::PageRole::Primary;
        playbackAccess().playback().stopWhenRequestReady = false;
        playbackAccess().playback().loopIterationsCompleted = 0;
        if (!provider.metadataReady) {
            playbackAccess().playback().providerStartPending = true;
            playbackAccess().request().roles[0].activeRequest.target = { -1, -1,
                ImageViewportInternal::ProviderRequestTargetKind::Playback };
            playbackAccess().request().roles[0].activeRequest.resolvedFrame = { -1, -1 };
            playbackAccess().playback().position = -1;
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
        } else if (provider.timedMetadata && provider.timedPlaybackSupport) {
            const int frame = playbackAccess().request().roles[0].activeRequest.target.frame;
            playbackAccess().playback().position = provider.timingIntervals.frameStartPosition(frame);
            playbackAccess().playback().phase
                = playbackAccess().request().status == ImageViewport::RequestStatus::Loading
                ? ImageViewport::PlaybackPhase::Waiting
                : ImageViewport::PlaybackPhase::Playing;
        }
        return;
    }
    if (!source.facts.timed || !source.facts.timingIntervals.isValid()) {
        return;
    }
    playbackAccess().playback().role = ImageViewport::PageRole::Primary;
    playbackAccess().playback().stopWhenRequestReady = false;
    playbackAccess().playback().loopIterationsCompleted = 0;
    playbackAccess().playback().position
        = source.facts.timingIntervals.frameStartPosition(playbackAccess().request().roles[0].activeRequest.target.frame);
    playbackAccess().playback().phase = playbackAccess().request().status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

ViewportEngine::PlaybackTickResult ViewportEngine::advancePlayback(
    const PlaybackTickInput& input)
{
    PlaybackTickResult result;
    if (playbackAccess().playback().phase != ImageViewport::PlaybackPhase::Playing
        || input.elapsedMilliseconds <= 0) {
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }

    const ImageViewport::PageRole role = playbackAccess().playback().role;
    auto& roleRequest = requestRole(playbackAccess().request(), role);
    const auto& source = roleRequest.source;
    auto& provider = playbackAccess().roles()[roleIndex(role)].provider;
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.timingIntervals : source.facts.timingIntervals;
    const auto authoredFacts
        = providerTiming ? provider.authoredAnimationFacts : source.facts.authoredAnimationFacts;
    if (!source.facts.present
        || (providerTiming
                ? (!provider.metadataReady || !provider.timedMetadata
                    || !provider.timedPlaybackSupport)
                : (!source.facts.timed || !intervals.isValid()))) {
        result.schedule = playbackScheduleEffect();
        return result;
    }

    const int currentFrame = roleRequest.activeRequest.target.frame;
    const auto target = ImageViewportInternal::playbackAdvanceTarget(
        input.elapsedMilliseconds, currentFrame, playbackAccess().playback().position,
        effectiveLooping(playbackAccess().playback(), authoredFacts), intervals.totalDuration(),
        intervals.frameCount(),
        [&intervals](int frame) { return intervals.frameStartPosition(frame); },
        [&intervals](int position) { return intervals.frameIndexForPosition(position); });
    if (!target.valid) {
        result.schedule = playbackScheduleEffect();
        return result;
    }

    playbackAccess().playback().position = target.playbackPosition;
    const bool sameReadyProviderFrame = providerTiming
        && target.displayTarget.frame == currentFrame
        && playbackAccess().request().status == ImageViewport::RequestStatus::Ready;
    if (sameReadyProviderFrame && role == ImageViewport::PageRole::Primary) {
        if (playbackAccess().playback().stopWhenRequestReady || target.reachedEnd) {
            playbackAccess().playback().stopWhenRequestReady = false;
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        } else if (target.looped && !playbackAccess().playback().looping) {
            ++playbackAccess().playback().loopIterationsCompleted;
        }
        result.schedule = playbackScheduleEffect();
        return result;
    }
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        result.schedule = playbackScheduleEffect();
        return result;
    }

    auto displayTarget = target.displayTarget;
    if (providerTiming) {
        displayTarget.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Playback;
    }

    if (role == ImageViewport::PageRole::Primary) {
        playbackAccess().request().beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
            displayTarget,
            { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) }, false);
    } else {
        const auto primaryRequest = playbackAccess().request().roles[0].activeRequest;
        playbackAccess().request().beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
            primaryRequest.target, primaryRequest.resolvedFrame, false);
        playbackAccess().request().roles[1].activeRequest.identity = playbackAccess().request().roles[0].activeRequest.identity;
        playbackAccess().request().roles[1].activeRequest.target = displayTarget;
        playbackAccess().request().roles[1].activeRequest.resolvedFrame
            = { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) };
        playbackAccess().request().roles[1].activeRequest.providerFrameToken = {};
        playbackAccess().request().roles[1].activeRequest.preparedPayloadId = 0;
    }
    if (target.looped && !playbackAccess().playback().looping) {
        ++playbackAccess().playback().loopIterationsCompleted;
    }

    if (providerTiming) {
        const std::size_t index = role == ImageViewport::PageRole::Secondary ? 1U : 0U;
        auto& effect = result.effects.providerFrameTransport[index];
        auto& active = requestRole(playbackAccess().request(), role).activeRequest;
        bool acceptedDispatch = true;
        if (provider.activeFrameToken.isValid()) {
            const auto queued = queueProviderFrameRequest(
                { role, displayTarget.frame, displayTarget.providerTargetKind });
            effect.cancelToken = queued.cancelToken;
            effect.deferredControllerEvent = queued.deferredFlush
                ? ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest
                : ViewportProviderDeferredControllerEvent::None;
        } else if (provider.nextRequestToken == std::numeric_limits<quint64>::max()) {
            effect.closeSession = provider.sessionActive;
            effect.sessionClose.metadataToken = provider.activeMetadataToken;
            effect.sessionClose.frameToken = provider.activeFrameToken;
            provider.activeMetadataToken = {};
            provider.activeFrameToken = {};
            provider.nextRequestToken = 0;
            playbackAccess().request().status = ImageViewport::RequestStatus::Error;
            playbackAccess().request().reason = ImageViewport::RequestReason::ProviderFailure;
            playbackAccess().request().errorString = QStringLiteral("provider request token exhausted");
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.diagnostics = true;
            acceptedDispatch = false;
        } else {
            provider.activeFrameToken
                = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(
                    ++provider.nextRequestToken);
            active.providerFrameToken = provider.activeFrameToken;
            effect.sendCommand = provider.sessionActive;
            effect.command.token = provider.activeFrameToken;
            effect.command.frame = active.resolvedFrame.frame;
            effect.command.position = active.target.position;
            effect.command.targetKind = active.target.providerTargetKind;
            effect.command.demand = providerDisplayDemand(role, input.geometry);
            playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
            playbackAccess().request().reason = ImageViewport::RequestReason::ProviderWaiting;
            playbackAccess().display().status = playbackAccess().display().roles[0].displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            playbackAccess().display().clearPendingRenderPayload();
            playbackAccess().display().clearRenderFailureRetainedDisplay();
        }
        if (acceptedDispatch) {
            playbackAccess().playback().stopWhenRequestReady = target.reachedEnd;
            playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;
        }
        result.changes.requestState = true;
        result.changes.requestRevision = true;
        result.changes.displayState = true;
        result.changes.displayRevision = true;
        result.changes.playbackPhase = true;
        result.changes.scheduleUpdate = true;
        result.schedule = playbackScheduleEffect();
        return result;
    }

    playbackAccess().display().captureRenderFailureRetainedDisplay(true);
    playbackAccess().display().roles[0].pendingRenderPayload.commitPending = true;
    playbackAccess().display().beginPreparedPayloadIdentity(
        playbackAccess().request().sequenceGeneration, playbackAccess().request().roles[0].activeRequest);
    const auto primaryAdmission = FramePreparation::admitBuiltInFrame(
        playbackAccess().request().roles[0].source, playbackAccess().request().roles[0].activeRequest.target.frame,
        playbackAccess().display().roles[0].pendingRenderPayload);
    playbackAccess().display().roles[0].pendingRenderPayload = primaryAdmission.preparedPayload;
    if (playbackAccess().request().roles[1].source.facts.present
        && !playbackAccess().request().roles[1].source.facts.provider
        && playbackAccess().request().roles[1].activeRequest.target.frame >= 0) {
        ImageViewportInternal::PreparedPayload secondaryPayload;
        secondaryPayload.commitPending = true;
        secondaryPayload.generation = playbackAccess().request().sequenceGeneration;
        secondaryPayload.requestId = playbackAccess().request().roles[0].activeRequest.identity.id;
        secondaryPayload.payloadId = ++playbackAccess().display().nextPreparedPayloadId;
        playbackAccess().request().roles[1].activeRequest.preparedPayloadId = secondaryPayload.payloadId;
        const auto secondaryAdmission = FramePreparation::admitBuiltInFrame(
            playbackAccess().request().roles[1].source,
            playbackAccess().request().roles[1].activeRequest.target.frame, secondaryPayload);
        playbackAccess().display().roles[1].pendingRenderPayload = secondaryAdmission.preparedPayload;
    }
    playbackAccess().request().status = ImageViewport::RequestStatus::Loading;
    playbackAccess().request().reason = input.geometry.itemBounds.isEmpty()
        ? ImageViewport::RequestReason::RenderWaiting
        : ImageViewport::RequestReason::UploadPending;
    playbackAccess().display().status = playbackAccess().display().roles[0].displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    playbackAccess().playback().stopWhenRequestReady = target.reachedEnd;
    playbackAccess().playback().phase = ImageViewport::PlaybackPhase::Waiting;

    result.changes.requestState = true;
    result.changes.requestRevision = true;
    result.changes.displayState = true;
    result.changes.displayRevision = true;
    result.changes.playbackPhase = true;
    result.changes.scheduleUpdate = true;
    result.schedule = playbackScheduleEffect();
    return result;
}

ViewportPlaybackScheduleEffect ViewportEngine::playbackScheduleEffect() const
{
    using Action = ViewportPlaybackScheduleEffect::Action;
    const auto state = snapshotAccess();
    if (state.playback().phase != ImageViewport::PlaybackPhase::Playing
        || state.request().status != ImageViewport::RequestStatus::Ready) {
        return { Action::Stop, -1 };
    }

    const ImageViewport::PageRole role = state.playback().role;
    const auto& roleRequest = requestRole(state.request(), role);
    const auto& source = roleRequest.source;
    const auto& provider = state.roles()[roleIndex(role)].provider;
    const bool providerTiming = source.facts.provider;
    const TimingIntervals& intervals
        = providerTiming ? provider.timingIntervals : source.facts.timingIntervals;
    const int frameCount
        = providerTiming ? intervals.frameCount() : source.facts.frameCount;
    const int totalDuration
        = providerTiming ? intervals.totalDuration() : source.facts.totalDuration;
    if (providerTiming && (!provider.metadataReady || !provider.timedMetadata)) {
        return { Action::Stop, -1 };
    }

    const int currentFrame = roleRequest.activeRequest.target.frame;
    if (currentFrame < 0 || currentFrame >= frameCount) {
        return { Action::Stop, -1 };
    }

    const int frameStart = intervals.frameStartPosition(currentFrame);
    const int nextFrameStart = currentFrame + 1 < frameCount
        ? intervals.frameStartPosition(currentFrame + 1)
        : totalDuration;
    const int frameDuration = nextFrameStart - frameStart;
    if (frameStart < 0 || frameDuration <= 0) {
        return { Action::Stop, -1 };
    }

    const int playbackPosition
        = state.playback().position >= 0 ? state.playback().position : frameStart;
    return { Action::ArmAfter,
        std::max(1, frameStart + frameDuration - playbackPosition) };
}
