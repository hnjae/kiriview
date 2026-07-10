#include "viewportengine_p.h"

#include "viewportcontrollerplaybackcontract_p.h"
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

bool effectiveLooping(const ImageViewportInternal::RequestState& request,
    ImageSequenceAuthoredAnimationFacts facts)
{
    if (request.looping) {
        return true;
    }
    switch (facts.loopMode()) {
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Infinite:
        return true;
    case ImageSequenceAuthoredAnimationFacts::LoopMode::Finite:
        return request.playbackLoopIterationsCompleted + 1 < facts.loopCount();
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
    if (!rolePresent(m_requestState, input.command.role)) {
        result.command = rejected(ImageViewport::CommandOutcome::IgnoredNoRequest,
            ImageViewport::CommandReason::IgnoredNoRequest);
        appendCommandChanges(result.command, result.changes);
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }
    const auto& terminal = m_requestState.targetSpreadTerminal;
    const auto generationTerminal = terminal.sealed
        && terminal.generation == m_requestState.sequenceGeneration
        && terminal.requestId == m_requestState.activeRequest.identity.id
        && ((terminal.primary.terminal
                && terminal.primary.failureScope
                    == ImageViewportInternal::FailureScope::Generation)
            || (terminal.secondary.terminal
                && terminal.secondary.failureScope
                    == ImageViewportInternal::FailureScope::Generation));
    if (generationTerminal
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
            m_requestState.beginDisplayRequest(origin, target, resolved, remember);
            return;
        }
        const auto primaryRequest = m_requestState.activeRequest;
        m_requestState.beginDisplayRequest(
            origin, primaryRequest.target, primaryRequest.resolvedFrame, false);
        auto& secondary = m_requestState.secondaryActiveRequest;
        secondary.identity = m_requestState.activeRequest.identity;
        secondary.target = target;
        secondary.resolvedFrame = resolved;
        secondary.providerFrameToken = {};
        secondary.preparedPayloadId = 0;
        if (remember) {
            m_requestState.secondaryLatestNonPlaybackRequest = secondary;
        }
    };
    auto stageBuiltIn = [this, &input]() {
        m_requestState.targetSpreadTerminal.clear();
        m_requestState.lastAcceptedRenderFailure = {};
        m_displayState.captureRenderFailureRetainedDisplay(true);
        m_displayState.pendingRenderPayload.commitPending = true;
        m_displayState.beginPreparedPayloadIdentity(
            m_requestState.sequenceGeneration, m_requestState.activeRequest);
        m_displayState.pendingRenderPayload
            = FramePreparation::admitBuiltInFrame(m_requestState.sequenceSource,
                  m_requestState.activeRequest.target.frame,
                  m_displayState.pendingRenderPayload)
                  .preparedPayload;
        if (m_requestState.secondarySequenceSource.facts.present
            && !m_requestState.secondarySequenceSource.facts.provider
            && m_requestState.secondaryActiveRequest.target.frame >= 0) {
            ImageViewportInternal::PreparedPayload payload;
            payload.commitPending = true;
            payload.generation = m_requestState.sequenceGeneration;
            payload.requestId = m_requestState.activeRequest.identity.id;
            payload.payloadId = ++m_displayState.nextPreparedPayloadId;
            m_requestState.secondaryActiveRequest.preparedPayloadId = payload.payloadId;
            m_displayState.secondaryPendingRenderPayload
                = FramePreparation::admitBuiltInFrame(
                      m_requestState.secondarySequenceSource,
                      m_requestState.secondaryActiveRequest.target.frame, payload)
                      .preparedPayload;
        }
        m_requestState.status = ImageViewport::RequestStatus::Loading;
        m_requestState.reason = input.geometry.itemBounds.isEmpty()
            ? ImageViewport::RequestReason::RenderWaiting
            : ImageViewport::RequestReason::UploadPending;
        m_displayState.status = m_displayState.displayedImageSize.isValid()
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
    auto dispatchProvider = [this, &result, &markRequest](ImageViewport::PageRole role,
                                ImageViewportInternal::DisplayRequestTarget target) {
        const std::size_t index = role == ImageViewport::PageRole::Secondary ? 1U : 0U;
        auto& effect = result.effects.providerFrameTransport[index];
        auto& provider = m_roles[roleIndex(role)].provider;
        auto& active = requestRole(m_requestState, role).activeRequest;
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
            effect.closeSession = provider.session != nullptr;
            effect.sessionClose.metadataToken = provider.activeMetadataToken;
            effect.sessionClose.frameToken = provider.activeFrameToken;
            provider.activeMetadataToken = {};
            provider.activeFrameToken = {};
            provider.nextRequestToken = 0;
            m_requestState.status = ImageViewport::RequestStatus::Error;
            m_requestState.reason = ImageViewport::RequestReason::ProviderFailure;
            m_requestState.errorString = QStringLiteral("provider request token exhausted");
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
            m_displayState.status = m_displayState.status == ImageViewport::DisplayStatus::Empty
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
        effect.sendCommand = provider.session != nullptr;
        effect.command.token = provider.activeFrameToken;
        effect.command.frame = active.resolvedFrame.frame;
        effect.command.position = active.target.position;
        effect.command.targetKind = active.target.providerTargetKind;
        m_requestState.status = ImageViewport::RequestStatus::Loading;
        m_requestState.reason = ImageViewport::RequestReason::ProviderWaiting;
        m_displayState.status = m_displayState.displayedImageSize.isValid()
            ? ImageViewport::DisplayStatus::Retained
            : ImageViewport::DisplayStatus::Empty;
        m_displayState.clearPendingRenderPayload();
        m_displayState.clearRenderFailureRetainedDisplay();
        return true;
    };

    if (input.command.kind == ViewportPlaybackCommand::Kind::Pause
        && (m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Stopped
            || m_requestState.playbackRole == input.command.role)
        && (m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Playing
            || m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Waiting)) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    } else if (input.command.kind == ViewportPlaybackCommand::Kind::Pause) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
    } else if (input.command.kind == ViewportPlaybackCommand::Kind::Play) {
        const auto& source = requestRole(m_requestState, input.command.role).source;
        auto& provider = m_roles[roleIndex(input.command.role)].provider;
        if (source.facts.provider && input.generationTerminalProviderFailure) {
            result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                ImageViewport::CommandReason::UnsupportedRequest);
            appendCommandChanges(result.command, result.changes);
            result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
            return result;
        }
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
            m_requestState.playbackRole = input.command.role;
            m_requestState.stopPlaybackWhenRequestReady = false;
            m_requestState.playbackLoopIterationsCompleted = 0;
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
                auto& secondary = m_requestState.secondaryActiveRequest;
                secondary.target = { -1, -1,
                    ImageViewportInternal::ProviderRequestTargetKind::Playback };
                secondary.resolvedFrame = { -1, -1 };
                secondary.providerFrameToken = {};
            }
            m_requestState.providerPlaybackStartPending
                = input.command.role == ImageViewport::PageRole::Primary;
            m_requestState.playbackPosition = -1;
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
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
            && (m_requestState.status == ImageViewport::RequestStatus::Unsupported
                || m_requestState.status == ImageViewport::RequestStatus::Error)) {
            result.changes.diagnostics = m_requestState.clearDiagnostics();
            stageBuiltIn();
            markRequest();
        }
        const bool preservePosition
            = m_requestState.playbackRole == input.command.role
            && m_requestState.playbackPosition >= 0
            && !m_requestState.stopPlaybackWhenRequestReady
            && m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Stopped;
        m_requestState.playbackRole = input.command.role;
        m_requestState.stopPlaybackWhenRequestReady = false;
        if (!preservePosition) {
            const int frame = requestRole(m_requestState, input.command.role)
                                  .activeRequest.target.frame;
            const auto& intervals
                = source.facts.provider ? provider.timingIntervals : source.facts.timingIntervals;
            m_requestState.playbackPosition = intervals.frameStartPosition(frame);
            m_requestState.playbackLoopIterationsCompleted = 0;
        }
        if (source.facts.provider
            && (m_requestState.status == ImageViewport::RequestStatus::Unsupported
                || m_requestState.status == ImageViewport::RequestStatus::Error)) {
            auto& active = requestRole(m_requestState, input.command.role).activeRequest;
            int frame = active.target.frame;
            if (frame < 0 || frame >= provider.timingIntervals.frameCount()) {
                frame = 0;
            }
            active.target = { frame, provider.timingIntervals.frameStartPosition(frame),
                ImageViewportInternal::ProviderRequestTargetKind::Playback };
            active.resolvedFrame = { frame, provider.timingIntervals.frameStartPosition(frame) };
            m_requestState.clearDiagnostics();
            m_requestState.targetSpreadTerminal.clear();
            dispatchProvider(input.command.role, active.target);
            markRequest();
        }
        const auto phase = m_requestState.status == ImageViewport::RequestStatus::Loading
            ? ImageViewport::PlaybackPhase::Waiting
            : ImageViewport::PlaybackPhase::Playing;
        if (m_requestState.playbackPhase != phase) {
            m_requestState.playbackPhase = phase;
            result.changes.playbackPhase = true;
        }
    } else if (input.command.kind == ViewportPlaybackCommand::Kind::Stop) {
        result.command = accepted();
        appendCommandChanges(result.command, result.changes);
        if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Stopped
            && m_requestState.playbackRole != input.command.role) {
            result.schedule = playbackScheduleEffect();
            return result;
        }
        m_requestState.stopPlaybackWhenRequestReady = false;
        auto& roleState = requestRole(m_requestState, input.command.role);
        const bool providerSource = roleState.source.facts.provider;
        auto& stopProvider = m_roles[roleIndex(input.command.role)].provider;
        if (providerSource && stopProvider.activeFrameToken.isValid()
            && m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Stopped
            && m_requestState.playbackRole == input.command.role) {
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
        if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Stopped
            && restore.identity.id != 0
            && (roleState.activeRequest.identity.origin
                    == ImageViewportInternal::DisplayRequestOrigin::Playback
                || roleState.activeRequest.target.providerTargetKind
                    == ImageViewportInternal::ProviderRequestTargetKind::Playback)) {
            beginRoleRequest(input.command.role,
                ImageViewportInternal::DisplayRequestOrigin::StopRestore, restore.target,
                restore.resolvedFrame, true);
            m_requestState.playbackPosition = restore.target.position;
            const auto& displayed = input.command.role == ImageViewport::PageRole::Primary
                ? m_displayState.displayedRequest
                : m_displayState.secondaryDisplayedRequest;
            const QSizeF displayedSize = input.command.role == ImageViewport::PageRole::Primary
                ? m_displayState.displayedImageSize
                : m_displayState.secondaryDisplayedImageSize;
            if (displayed.generation == m_requestState.sequenceGeneration
                && displayed.request.resolvedFrame.frame == restore.resolvedFrame.frame
                && displayed.request.resolvedFrame.position == restore.resolvedFrame.position
                && displayedSize.isValid()
                && (!providerSource || restore.resolvedFrame.isValid())) {
                m_requestState.status = ImageViewport::RequestStatus::Ready;
                m_requestState.reason = ImageViewport::RequestReason::Ready;
                m_displayState.status = ImageViewport::DisplayStatus::Ready;
                result.changes.requestState = true;
                result.changes.requestRevision = true;
                result.changes.displayState = true;
                result.changes.displayRevision = true;
            } else {
                if (providerSource && restore.target.frame >= 0
                    && stopProvider.metadataReady) {
                    dispatchProvider(input.command.role, restore.target);
                } else if (providerSource) {
                    m_requestState.status = ImageViewport::RequestStatus::Loading;
                    m_requestState.reason = ImageViewport::RequestReason::ProviderWaiting;
                    m_displayState.status = m_displayState.displayedImageSize.isValid()
                        ? ImageViewport::DisplayStatus::Retained
                        : ImageViewport::DisplayStatus::Empty;
                } else {
                    stageBuiltIn();
                }
                markRequest();
            }
        }
        if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Stopped) {
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        }
    } else {
        const auto& source = requestRole(m_requestState, input.command.role).source;
        auto& provider = m_roles[roleIndex(input.command.role)].provider;
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
                if (m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Playing) {
                    m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
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
            if (input.generationTerminalProviderFailure) {
                result.command = rejected(ImageViewport::CommandOutcome::Unsupported,
                    ImageViewport::CommandReason::UnsupportedRequest);
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
            result.changes.diagnostics = m_requestState.clearDiagnostics();
            dispatchProvider(input.command.role,
                requestRole(m_requestState, input.command.role).activeRequest.target);
            markRequest();
            if (m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Playing) {
                m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
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
        result.changes.diagnostics = m_requestState.clearDiagnostics();
        stageBuiltIn();
        markRequest();
        if (m_requestState.playbackPhase == ImageViewport::PlaybackPhase::Playing) {
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
            result.changes.playbackPhase = true;
        }
    }
    result.schedule = playbackScheduleEffect();
    return result;
}

void ViewportEngine::setPlaybackPhase(ImageViewport::PlaybackPhase phase,
    ImageViewportInternal::ViewportChangeSet& changes)
{
    if (m_requestState.playbackPhase == phase) {
        return;
    }
    m_requestState.playbackPhase = phase;
    changes.playbackPhase = true;
}

void ViewportEngine::armAuthoredAutoplayIfEligible()
{
    const auto& source = m_requestState.sequenceSource;
    const auto& provider = m_roles[0].provider;
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
        m_requestState.playbackRole = ImageViewport::PageRole::Primary;
        m_requestState.stopPlaybackWhenRequestReady = false;
        m_requestState.playbackLoopIterationsCompleted = 0;
        if (!provider.metadataReady) {
            m_requestState.providerPlaybackStartPending = true;
            m_requestState.activeRequest.target = { -1, -1,
                ImageViewportInternal::ProviderRequestTargetKind::Playback };
            m_requestState.activeRequest.resolvedFrame = { -1, -1 };
            m_requestState.playbackPosition = -1;
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
        } else if (provider.timedMetadata && provider.timedPlaybackSupport) {
            const int frame = m_requestState.activeRequest.target.frame;
            m_requestState.playbackPosition = provider.timingIntervals.frameStartPosition(frame);
            m_requestState.playbackPhase
                = m_requestState.status == ImageViewport::RequestStatus::Loading
                ? ImageViewport::PlaybackPhase::Waiting
                : ImageViewport::PlaybackPhase::Playing;
        }
        return;
    }
    if (!source.facts.timed || !source.facts.timingIntervals.isValid()) {
        return;
    }
    m_requestState.playbackRole = ImageViewport::PageRole::Primary;
    m_requestState.stopPlaybackWhenRequestReady = false;
    m_requestState.playbackLoopIterationsCompleted = 0;
    m_requestState.playbackPosition
        = source.facts.timingIntervals.frameStartPosition(m_requestState.activeRequest.target.frame);
    m_requestState.playbackPhase = m_requestState.status == ImageViewport::RequestStatus::Loading
        ? ImageViewport::PlaybackPhase::Waiting
        : ImageViewport::PlaybackPhase::Playing;
}

ViewportEngine::PlaybackTickResult ViewportEngine::advancePlayback(
    const PlaybackTickInput& input)
{
    PlaybackTickResult result;
    if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Playing
        || input.elapsedMilliseconds <= 0) {
        result.schedule = { ViewportPlaybackScheduleEffect::Action::NoChange, -1 };
        return result;
    }

    const ImageViewport::PageRole role = m_requestState.playbackRole;
    auto& roleRequest = requestRole(m_requestState, role);
    const auto& source = roleRequest.source;
    auto& provider = m_roles[roleIndex(role)].provider;
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
        input.elapsedMilliseconds, currentFrame, m_requestState.playbackPosition,
        effectiveLooping(m_requestState, authoredFacts), intervals.totalDuration(),
        intervals.frameCount(),
        [&intervals](int frame) { return intervals.frameStartPosition(frame); },
        [&intervals](int position) { return intervals.frameIndexForPosition(position); });
    if (!target.valid) {
        result.schedule = playbackScheduleEffect();
        return result;
    }

    m_requestState.playbackPosition = target.playbackPosition;
    const bool sameReadyProviderFrame = providerTiming
        && target.displayTarget.frame == currentFrame
        && m_requestState.status == ImageViewport::RequestStatus::Ready;
    if (sameReadyProviderFrame && role == ImageViewport::PageRole::Primary) {
        if (m_requestState.stopPlaybackWhenRequestReady || target.reachedEnd) {
            m_requestState.stopPlaybackWhenRequestReady = false;
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.playbackPhase = true;
        } else if (target.looped && !m_requestState.looping) {
            ++m_requestState.playbackLoopIterationsCompleted;
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
        m_requestState.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
            displayTarget,
            { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) }, false);
    } else {
        const auto primaryRequest = m_requestState.activeRequest;
        m_requestState.beginDisplayRequest(ImageViewportInternal::DisplayRequestOrigin::Playback,
            primaryRequest.target, primaryRequest.resolvedFrame, false);
        m_requestState.secondaryActiveRequest.identity = m_requestState.activeRequest.identity;
        m_requestState.secondaryActiveRequest.target = displayTarget;
        m_requestState.secondaryActiveRequest.resolvedFrame
            = { displayTarget.frame, intervals.frameStartPosition(displayTarget.frame) };
        m_requestState.secondaryActiveRequest.providerFrameToken = {};
        m_requestState.secondaryActiveRequest.preparedPayloadId = 0;
    }
    if (target.looped && !m_requestState.looping) {
        ++m_requestState.playbackLoopIterationsCompleted;
    }

    if (providerTiming) {
        const std::size_t index = role == ImageViewport::PageRole::Secondary ? 1U : 0U;
        auto& effect = result.effects.providerFrameTransport[index];
        auto& active = requestRole(m_requestState, role).activeRequest;
        bool acceptedDispatch = true;
        if (provider.activeFrameToken.isValid()) {
            const auto queued = queueProviderFrameRequest(
                { role, displayTarget.frame, displayTarget.providerTargetKind });
            effect.cancelToken = queued.cancelToken;
            effect.deferredControllerEvent = queued.deferredFlush
                ? ViewportProviderDeferredControllerEvent::FlushQueuedFrameRequest
                : ViewportProviderDeferredControllerEvent::None;
        } else if (provider.nextRequestToken == std::numeric_limits<quint64>::max()) {
            effect.closeSession = provider.session != nullptr;
            effect.sessionClose.metadataToken = provider.activeMetadataToken;
            effect.sessionClose.frameToken = provider.activeFrameToken;
            provider.activeMetadataToken = {};
            provider.activeFrameToken = {};
            provider.nextRequestToken = 0;
            m_requestState.status = ImageViewport::RequestStatus::Error;
            m_requestState.reason = ImageViewport::RequestReason::ProviderFailure;
            m_requestState.errorString = QStringLiteral("provider request token exhausted");
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Stopped;
            result.changes.diagnostics = true;
            acceptedDispatch = false;
        } else {
            provider.activeFrameToken
                = ImageViewportInternal::ProviderRequestTokenPrivateAccess::fromValue(
                    ++provider.nextRequestToken);
            active.providerFrameToken = provider.activeFrameToken;
            effect.sendCommand = provider.session != nullptr;
            effect.command.token = provider.activeFrameToken;
            effect.command.frame = active.resolvedFrame.frame;
            effect.command.position = active.target.position;
            effect.command.targetKind = active.target.providerTargetKind;
            m_requestState.status = ImageViewport::RequestStatus::Loading;
            m_requestState.reason = ImageViewport::RequestReason::ProviderWaiting;
            m_displayState.status = m_displayState.displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            m_displayState.clearPendingRenderPayload();
            m_displayState.clearRenderFailureRetainedDisplay();
        }
        if (acceptedDispatch) {
            m_requestState.stopPlaybackWhenRequestReady = target.reachedEnd;
            m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;
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

    m_displayState.captureRenderFailureRetainedDisplay(true);
    m_displayState.pendingRenderPayload.commitPending = true;
    m_displayState.beginPreparedPayloadIdentity(
        m_requestState.sequenceGeneration, m_requestState.activeRequest);
    const auto primaryAdmission = FramePreparation::admitBuiltInFrame(
        m_requestState.sequenceSource, m_requestState.activeRequest.target.frame,
        m_displayState.pendingRenderPayload);
    m_displayState.pendingRenderPayload = primaryAdmission.preparedPayload;
    if (m_requestState.secondarySequenceSource.facts.present
        && !m_requestState.secondarySequenceSource.facts.provider
        && m_requestState.secondaryActiveRequest.target.frame >= 0) {
        ImageViewportInternal::PreparedPayload secondaryPayload;
        secondaryPayload.commitPending = true;
        secondaryPayload.generation = m_requestState.sequenceGeneration;
        secondaryPayload.requestId = m_requestState.activeRequest.identity.id;
        secondaryPayload.payloadId = ++m_displayState.nextPreparedPayloadId;
        m_requestState.secondaryActiveRequest.preparedPayloadId = secondaryPayload.payloadId;
        const auto secondaryAdmission = FramePreparation::admitBuiltInFrame(
            m_requestState.secondarySequenceSource,
            m_requestState.secondaryActiveRequest.target.frame, secondaryPayload);
        m_displayState.secondaryPendingRenderPayload = secondaryAdmission.preparedPayload;
    }
    m_requestState.status = ImageViewport::RequestStatus::Loading;
    m_requestState.reason = input.geometry.itemBounds.isEmpty()
        ? ImageViewport::RequestReason::RenderWaiting
        : ImageViewport::RequestReason::UploadPending;
    m_displayState.status = m_displayState.displayedImageSize.isValid()
        ? ImageViewport::DisplayStatus::Retained
        : ImageViewport::DisplayStatus::Empty;
    m_requestState.stopPlaybackWhenRequestReady = target.reachedEnd;
    m_requestState.playbackPhase = ImageViewport::PlaybackPhase::Waiting;

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
    if (m_requestState.playbackPhase != ImageViewport::PlaybackPhase::Playing
        || m_requestState.status != ImageViewport::RequestStatus::Ready) {
        return { Action::Stop, -1 };
    }

    const ImageViewport::PageRole role = m_requestState.playbackRole;
    const auto& roleRequest = requestRole(m_requestState, role);
    const auto& source = roleRequest.source;
    const auto& provider = m_roles[roleIndex(role)].provider;
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
        = m_requestState.playbackPosition >= 0 ? m_requestState.playbackPosition : frameStart;
    return { Action::ArmAfter,
        std::max(1, frameStart + frameDuration - playbackPosition) };
}
