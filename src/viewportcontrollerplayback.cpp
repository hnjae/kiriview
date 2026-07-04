#include "viewportcontrollerhelpers_p.h"

ViewportCommandResult ViewportController::play()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata
        && viewportProviderState(viewport).timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(viewportRequestState(viewport).playbackPhase,
                  viewportRequestState(viewport).stopPlaybackWhenRequestReady)
            && viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Primary
            && viewportRequestState(viewport).playbackPosition >= 0;
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Unsupported
            || viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error) {
            const DisplayRequestTarget target = providerPlaybackStartTarget(viewport);
            clearCommandDiagnosticForAcceptedCommand(viewport, result);
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            applyProviderPlaybackStartTarget(viewport, target);
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            publishProviderFrameLoadingState(viewport);
            const ViewportProviderFrameDispatchResult dispatch
                = dispatchProviderFrameRequest({ target });
            result.providerFrameTransport = dispatch.transport;
            if (!dispatch.accepted) {
                result.changes.requestRevision = true;
                result.changes.requestState = true;
                result.changes.diagnostics = true;
                return result;
            }
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
            result.changes.requestRevision = true;
            result.changes.requestState = true;
            result.changes.diagnostics = diagnosticsValueChanged;
            return result;
        }

        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.providerFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerTimedPlaybackCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        applyPendingProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (viewport.hasTimedSequence()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        const bool preservePlaybackPosition
            = shouldPreservePlaybackPositionOnPlay(viewportRequestState(viewport).playbackPhase,
                  viewportRequestState(viewport).stopPlaybackWhenRequestReady)
            && viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Primary
            && viewportRequestState(viewport).playbackPosition >= 0;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Primary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Unsupported
            || viewportRequestState(viewport).status == ImageViewport::RequestStatus::Error) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus
                = viewportDisplayState(viewport).status;
            const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
            publishAcceptedTargetState(viewport);
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
            setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.diagnostics = diagnosticsValueChanged;
            result.changes.scheduleUpdate = true;
            return result;
        }
        if (!preservePlaybackPosition) {
            seedPlaybackPosition(
                viewport, [this](int frame) { return viewport.sequenceFrameStartPosition(frame); });
            viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        }
        setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::play(ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (role == ImageViewport::PageRole::Primary) {
        return play();
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (viewportRequestState(viewport).secondarySequenceIsProvider) {
        return commandResultWithSecondaryTransport(playSecondaryProvider());
    }
    if (!state.secondarySource.timed) {
        return rejectUnsupportedCommand();
    }
    return playSecondaryBuiltIn();
}

ViewportCommandResult ViewportController::playSecondaryBuiltIn()
{
    if (!viewport.hasActiveRequest()
        || viewportRequestState(viewport).secondaryActiveRequest.target.frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Secondary;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    viewportRequestState(viewport).playbackPosition
        = viewportRequestState(viewport).secondaryActiveRequest.target.position >= 0
        ? viewportRequestState(viewport).secondaryActiveRequest.target.position
        : viewportRequestState(viewport).secondaryActiveRequest.resolvedFrame.position;
    setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
    return result;
}

ViewportCommandResult ViewportController::playSecondaryProvider()
{
    const ImageViewportInternal::DisplayRequest& request
        = viewportRequestState(viewport).secondaryActiveRequest;
    const bool currentIdentity = request.identity.id != 0
        && request.identity.id == viewportRequestState(viewport).activeRequest.identity.id;
    if (!viewport.hasActiveRequest() || !currentIdentity) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (!state.secondaryProvider.metadataReady) {
        if (ImageViewportInternal::providerCapabilityKnownFalse(
                viewport.secondaryProviderTimedPlaybackCapability())) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Secondary;
        viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
        viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
        applyPendingSecondaryProviderPlaybackTarget(viewport, pendingProviderPlaybackTarget());
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
        result.changes.requestRevision = true;
        result.changes.requestState = true;
        return result;
    }

    if (request.target.frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (!state.secondaryProvider.timedMetadata || !state.secondaryProvider.timedPlaybackSupport) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    viewportRequestState(viewport).playbackRole = ImageViewport::PageRole::Secondary;
    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    viewportRequestState(viewport).playbackLoopIterationsCompleted = 0;
    viewportRequestState(viewport).playbackPosition
        = request.target.position >= 0 ? request.target.position : request.resolvedFrame.position;
    setPlaybackPhase(viewport, result, playbackPhaseForCurrentRequest(viewport));
    return result;
}

ViewportCommandResult ViewportController::pause()
{
    return pause(ImageViewport::PageRole::Primary);
}

ViewportCommandResult ViewportController::pause(ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (role == ImageViewport::PageRole::Secondary && !hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped
        && viewportRequestState(viewport).playbackRole != role) {
        return result;
    }
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewportRequestState(viewport).playbackPhase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportCommandResult ViewportController::stop() { return stop(ImageViewport::PageRole::Primary); }

ViewportCommandResult ViewportController::stop(ImageViewport::PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (role == ImageViewport::PageRole::Secondary && !hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Stopped
        && viewportRequestState(viewport).playbackRole != role) {
        return result;
    }

    viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
    if (role == ImageViewport::PageRole::Secondary) {
        if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Stopped) {
            return result;
        }

        if (activeProviderFrameRequestIsPlayback(
                state, viewport, ImageViewport::PageRole::Secondary)) {
            result.secondaryProviderFrameTransport.cancelToken
                = state.secondaryProvider.activeFrameToken;
            state.secondaryProvider.activeFrameToken = {};
        }

        const ImageViewportInternal::DisplayRequest restoreRequest
            = viewportRequestState(viewport).secondaryLatestNonPlaybackRequest;
        if (restoreRequest.target.frame >= 0) {
            const QRectF oldContentRect = viewport.contentRect();
            const QRectF oldVisibleImageRect = viewport.visibleImageRect();
            const ImageViewport::DisplayStatus oldDisplayStatus
                = viewportDisplayState(viewport).status;
            const ImageViewportInternal::DisplayRequest primaryRequest
                = viewportRequestState(viewport).activeRequest;
            beginAcceptedDisplayRequest(viewport,
                ImageViewportInternal::DisplayRequestOrigin::StopRestore, primaryRequest.target,
                primaryRequest.resolvedFrame, false);
            setSecondaryActiveRequest(
                viewport, restoreRequest.target, restoreRequest.resolvedFrame, true);
            viewportRequestState(viewport).playbackPosition = restoreRequest.target.position;
            if (displayedPrimaryPayloadMatchesActiveTarget(viewport)
                && displayedSecondaryPayloadMatchesActiveTarget(viewport)) {
                publishReadyDisplayState(viewport);
            } else {
                publishAcceptedTargetState(viewport);
            }
            setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
            result.changes.requestRevision = true;
            const bool displayValueChanged
                = viewportDisplayState(viewport).status != oldDisplayStatus
                || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
            result.changes.displayRevision = displayValueChanged;
            result.changes.requestState = true;
            result.changes.displayState = displayValueChanged;
            result.changes.geometryState
                = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
                || ImageViewportInternal::rectsDifferExactly(
                    viewport.visibleImageRect(), oldVisibleImageRect);
            result.changes.scheduleUpdate = true;
            return result;
        }

        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
        return result;
    }

    const StopRestorePlan stopRestorePlan = stopRestorePlanFor(viewport);
    if (applyStopRestorePlan(*this, viewport, stopRestorePlan, result)) {
        return result;
    }
    setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Stopped);
    return result;
}

ViewportCommandResult ViewportController::seek(int frame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasDisplayableSequence()) {
        if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady) {
            if (!viewportProviderState(viewport).frameSeekSupport) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            const int maximumFrame = viewportProviderState(viewport).timedMetadata
                ? viewportProviderState(viewport).timingIntervals.frameCount() - 1
                : 0;
            if (frame > maximumFrame) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget { frame, viewport.providerFrameStartPosition(frame),
                    ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderReady);
        }

        if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
            if (viewport.providerFrameSeekCapabilityKnownFalse()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (viewport.providerKnownFactsTimedFrameCount()
                && viewport.providerFrameSeekCapabilityKnownTrue()) {
                const int maximumFrame = viewport.providerKnownFactsFrameCount() - 1;
                if (frame > maximumFrame) {
                    ViewportCommandResult result;
                    result.outcome = ImageViewport::CommandOutcome::Invalid;
                    setCommandDiagnostic(
                        viewport, result, ImageViewport::CommandReason::InvalidRequest);
                    return result;
                }
            }

            return acceptExplicitSeek(*this, viewport,
                DisplayRequestTarget {
                    frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame },
                ExplicitSeekMaterialization::ProviderPendingMetadata);
        }

        if (frame >= viewport.sequenceFrameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget { frame,
                viewport.hasTimedSequence() ? viewport.sequenceFrameStartPosition(frame) : -1 },
            ExplicitSeekMaterialization::BuiltIn);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seek(ImageViewport::PageRole role, int frame)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (role == ImageViewport::PageRole::Primary) {
        return seek(frame);
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (viewportRequestState(viewport).secondarySequenceIsProvider) {
        return commandResultWithSecondaryTransport(seekSecondaryProvider(frame));
    }
    if (frame < 0) {
        return rejectInvalidCommand();
    }

    const int frameCount = state.secondarySource.frameCount;
    if (frameCount <= 0 || frame >= frameCount) {
        return rejectInvalidCommand();
    }

    const int position = frameStartPositionForRoleSource(viewport, state.secondarySource, frame);
    return seekSecondaryBuiltIn(
        { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
        { frame, position });
}

ViewportCommandResult ViewportController::seekSecondaryBuiltIn(
    ImageViewportInternal::DisplayRequestTarget target,
    ImageViewportInternal::ResolvedFrameIdentity resolvedFrame)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    const ImageViewport::DisplayStatus oldDisplayStatus = viewportDisplayState(viewport).status;
    const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
    const ImageViewportInternal::DisplayRequest primaryRequest
        = viewportRequestState(viewport).activeRequest;
    beginAcceptedDisplayRequest(viewport, ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek,
        primaryRequest.target, primaryRequest.resolvedFrame, true);
    setSecondaryActiveRequest(viewport, target, resolvedFrame, true);
    publishAcceptedTargetState(viewport);
    if (viewportRequestState(viewport).playbackPhase == ImageViewport::PlaybackPhase::Playing
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        setPlaybackPhase(viewport, result, ImageViewport::PlaybackPhase::Waiting);
    }

    result.changes.requestRevision = true;
    const bool displayValueChanged = viewportDisplayState(viewport).status != oldDisplayStatus
        || viewportDisplayState(viewport).status == ImageViewport::DisplayStatus::Ready;
    result.changes.displayRevision = displayValueChanged;
    result.changes.requestState = true;
    result.changes.displayState = displayValueChanged;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportCommandResult ViewportController::seekSecondaryProvider(int frame)
{
    if (!viewport.hasActiveRequest() || !hasSecondaryProviderSequence(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (frame < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto acceptTarget = [this](ImageViewportInternal::DisplayRequestTarget target,
                                  ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
                                  bool dispatchNow) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(viewport,
            ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, primaryRequest.target,
            primaryRequest.resolvedFrame, true);
        setSecondaryActiveRequest(viewport, target, resolvedFrame, true);

        if (dispatchNow) {
            if (state.secondaryProvider.session
                && state.secondaryProvider.activeFrameToken.isValid()) {
                result.providerFrameTransport.cancelToken
                    = state.secondaryProvider.activeFrameToken;
            }
            state.secondaryProvider.activeFrameToken = {};
            publishProviderFrameLoadingState(viewport);
            const ViewportProviderFrameRequestStartResult start
                = startSecondaryProviderFrameRequest(target);
            appendProviderFrameStartResult(result.providerFrameTransport, start);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!start.accepted) {
                result.changes.diagnostics = true;
            }
        } else {
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
            discardPendingRenderCommit(viewport);
        }

        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsValueChanged;
        return result;
    };

    if (state.secondaryProvider.metadataReady) {
        if (!state.secondaryProvider.frameSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int maximumFrame = state.secondaryProvider.timedMetadata
            ? state.secondaryProvider.timingIntervals.frameCount() - 1
            : 0;
        if (frame > maximumFrame) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        const int position = state.secondaryProvider.timedMetadata
            ? state.secondaryProvider.timingIntervals.frameStartPosition(frame)
            : -1;
        return acceptTarget(
            { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { frame, position }, true);
    }

    if (viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport frameSeekCapability
            = viewport.secondaryProviderFrameSeekCapability();
        if (ImageViewportInternal::providerCapabilityKnownFalse(frameSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts = viewport.secondaryProviderKnownFacts();
        if (ImageViewportInternal::providerCapabilityKnownTrue(frameSeekCapability)
            && knownFacts.frameCount() >= 0 && frame >= knownFacts.frameCount()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptTarget({ frame, -1, ImageViewportInternal::ProviderRequestTargetKind::Frame },
            { -1, -1 }, false);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    if (viewport.hasProviderSequence() && !viewportProviderState(viewport).metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        if (viewport.providerPositionSeekCapabilityKnownFalse()) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { -1, -1 }, ExplicitSeekMaterialization::ProviderPendingMetadata);
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        if (!viewportProviderState(viewport).positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame = viewport.providerFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport,
            DisplayRequestTarget {
                frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { frame, viewport.providerFrameStartPosition(frame) },
            ExplicitSeekMaterialization::ProviderReady);
    }

    if (viewport.hasTimedSequence()) {
        const int frame = viewport.sequenceFrameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }

        return acceptExplicitSeek(*this, viewport, DisplayRequestTarget { frame, milliseconds },
            { frame, viewport.sequenceFrameStartPosition(frame) },
            ExplicitSeekMaterialization::BuiltIn);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

ViewportCommandResult ViewportController::seekToPosition(
    ImageViewport::PageRole role, int milliseconds)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return rejectInvalidCommand();
    }
    if (role == ImageViewport::PageRole::Primary) {
        return seekToPosition(milliseconds);
    }
    if (!hasSecondaryRole(viewport)) {
        return rejectIgnoredNoRequestCommand();
    }
    if (viewportRequestState(viewport).secondarySequenceIsProvider) {
        return commandResultWithSecondaryTransport(seekSecondaryProviderToPosition(milliseconds));
    }
    if (milliseconds < 0) {
        return rejectInvalidCommand();
    }
    if (!state.secondarySource.timed) {
        return rejectUnsupportedCommand();
    }

    const int frame = frameIndexForRoleSource(viewport, state.secondarySource, milliseconds);
    if (frame < 0) {
        return rejectInvalidCommand();
    }
    const int frameStart = frameStartPositionForRoleSource(viewport, state.secondarySource, frame);
    return seekSecondaryBuiltIn(
        { frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
        { frame, frameStart });
}

ViewportCommandResult ViewportController::seekSecondaryProviderToPosition(int milliseconds)
{
    if (!viewport.hasActiveRequest() || !hasSecondaryProviderSequence(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }
    if (milliseconds < 0) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Invalid;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
        return result;
    }
    if (hasGenerationTerminalProviderFailure(viewport)) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Unsupported;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
        return result;
    }

    const auto acceptTarget = [this](ImageViewportInternal::DisplayRequestTarget target,
                                  ImageViewportInternal::ResolvedFrameIdentity resolvedFrame,
                                  bool dispatchNow) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::Accepted;
        clearCommandDiagnosticForAcceptedCommand(viewport, result);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(viewport,
            ImageViewportInternal::DisplayRequestOrigin::ExplicitSeek, primaryRequest.target,
            primaryRequest.resolvedFrame, true);
        setSecondaryActiveRequest(viewport, target, resolvedFrame, true);

        if (dispatchNow) {
            if (state.secondaryProvider.session
                && state.secondaryProvider.activeFrameToken.isValid()) {
                result.providerFrameTransport.cancelToken
                    = state.secondaryProvider.activeFrameToken;
            }
            state.secondaryProvider.activeFrameToken = {};
            publishProviderFrameLoadingState(viewport);
            const ViewportProviderFrameRequestStartResult start
                = startSecondaryProviderFrameRequest(target);
            appendProviderFrameStartResult(result.providerFrameTransport, start);
            result.changes.displayRevision = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!start.accepted) {
                result.changes.diagnostics = true;
            }
        } else {
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
            discardPendingRenderCommit(viewport);
        }

        result.changes.requestRevision = true;
        result.changes.requestState = true;
        result.changes.diagnostics = result.changes.diagnostics || diagnosticsValueChanged;
        return result;
    };

    if (!state.secondaryProvider.metadataReady
        && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Loading) {
        const ImageSequenceProviderCapabilitySupport positionSeekCapability
            = viewport.secondaryProviderPositionSeekCapability();
        if (ImageViewportInternal::providerCapabilityKnownFalse(positionSeekCapability)) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const ImageSequenceProviderKnownFacts knownFacts = viewport.secondaryProviderKnownFacts();
        if (ImageViewportInternal::providerCapabilityKnownTrue(positionSeekCapability)) {
            if (knownFacts.isStill()) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Unsupported;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
                return result;
            }
            if (knownFacts.isTimedFrameList()
                && TimingIntervals::fromFrameDurations(knownFacts.frameDurations())
                        .frameIndexForPosition(milliseconds)
                    < 0) {
                ViewportCommandResult result;
                result.outcome = ImageViewport::CommandOutcome::Invalid;
                setCommandDiagnostic(
                    viewport, result, ImageViewport::CommandReason::InvalidRequest);
                return result;
            }
        }

        return acceptTarget(
            { -1, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { -1, -1 }, false);
    }

    if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
        if (!state.secondaryProvider.positionSeekSupport) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Unsupported;
            setCommandDiagnostic(
                viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
            return result;
        }
        const int frame
            = state.secondaryProvider.timingIntervals.frameIndexForPosition(milliseconds);
        if (frame < 0) {
            ViewportCommandResult result;
            result.outcome = ImageViewport::CommandOutcome::Invalid;
            setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::InvalidRequest);
            return result;
        }
        const int frameStart = state.secondaryProvider.timingIntervals.frameStartPosition(frame);
        return acceptTarget(
            { frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Position },
            { frame, frameStart }, true);
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Unsupported;
    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::UnsupportedRequest);
    return result;
}

int ViewportController::playbackTimerInterval() const
{
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Playing
        || viewportRequestState(viewport).status != ImageViewport::RequestStatus::Ready) {
        return -1;
    }

    if (viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Secondary) {
        if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
            const int currentFrame
                = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
            if (currentFrame < 0
                || currentFrame >= state.secondaryProvider.timingIntervals.frameCount()) {
                return -1;
            }
            const int frameStart
                = state.secondaryProvider.timingIntervals.frameStartPosition(currentFrame);
            const int nextFrameStart
                = currentFrame + 1 < state.secondaryProvider.timingIntervals.frameCount()
                ? state.secondaryProvider.timingIntervals.frameStartPosition(currentFrame + 1)
                : state.secondaryProvider.timingIntervals.totalDuration();
            const int frameDuration = nextFrameStart - frameStart;
            if (frameStart < 0 || frameDuration <= 0) {
                return -1;
            }

            const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
                ? viewportRequestState(viewport).playbackPosition
                : frameStart;
            const int remaining = frameStart + frameDuration - playbackPosition;
            return std::max(1, remaining);
        }

        if (!viewport.hasSecondaryTimedSequence()) {
            return -1;
        }
        const int currentFrame = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
        if (currentFrame < 0 || currentFrame >= viewport.secondarySequenceFrameCount()) {
            return -1;
        }
        const int frameStart = viewport.secondarySequenceFrameStartPosition(currentFrame);
        const int nextFrameStart = currentFrame + 1 < viewport.secondarySequenceFrameCount()
            ? viewport.secondarySequenceFrameStartPosition(currentFrame + 1)
            : viewport.secondaryTotalDuration();
        const int frameDuration = nextFrameStart - frameStart;
        if (frameStart < 0 || frameDuration <= 0) {
            return -1;
        }

        const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
            ? viewportRequestState(viewport).playbackPosition
            : frameStart;
        const int remaining = frameStart + frameDuration - playbackPosition;
        return std::max(1, remaining);
    }

    int frameStart = -1;
    int frameDuration = -1;
    const int currentFrame = viewportRequestState(viewport).activeRequest.target.frame;
    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        if (currentFrame < 0 || currentFrame >= viewport.frameCount()) {
            return -1;
        }
        frameStart = viewport.providerFrameStartPosition(currentFrame);
        frameDuration = viewportProviderState(viewport).timingIntervals.frameDuration(currentFrame);
    } else if (viewport.hasTimedSequence()) {
        if (currentFrame < 0 || currentFrame >= viewport.sequenceFrameCount()) {
            return -1;
        }
        frameStart = viewport.sequenceFrameStartPosition(currentFrame);
        const int nextFrameStart = currentFrame + 1 < viewport.sequenceFrameCount()
            ? viewport.sequenceFrameStartPosition(currentFrame + 1)
            : viewport.totalDuration();
        frameDuration = nextFrameStart - frameStart;
    } else {
        return -1;
    }

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition = viewportRequestState(viewport).playbackPosition >= 0
        ? viewportRequestState(viewport).playbackPosition
        : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

ViewportPlaybackAdvanceResult ViewportController::advancePlayback(int elapsedMilliseconds)
{
    ViewportPlaybackAdvanceResult result;
    if (viewportRequestState(viewport).playbackPhase != ImageViewport::PlaybackPhase::Playing
        || elapsedMilliseconds <= 0) {
        return result;
    }

    if (viewportRequestState(viewport).playbackRole == ImageViewport::PageRole::Secondary) {
        if (state.secondaryProvider.metadataReady && state.secondaryProvider.timedMetadata) {
            const int totalDuration = state.secondaryProvider.timingIntervals.totalDuration();
            const int previousFrame
                = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
            const int currentFrame
                = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
            const PlaybackAdvanceTarget target = playbackAdvanceTarget(
                elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
                effectiveLoopingForPlayback(
                    viewport, state.secondaryProvider.authoredAnimationFacts),
                totalDuration, state.secondaryProvider.timingIntervals.frameCount(),
                [this](int frame) {
                    return state.secondaryProvider.timingIntervals.frameStartPosition(frame);
                },
                [this](int position) {
                    return state.secondaryProvider.timingIntervals.frameIndexForPosition(position);
                });
            if (!target.valid) {
                return result;
            }

            viewportRequestState(viewport).playbackPosition = target.playbackPosition;
            if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame
                && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready) {
                return result;
            }

            const ImageViewportInternal::DisplayRequest primaryRequest
                = viewportRequestState(viewport).activeRequest;
            beginAcceptedDisplayRequest(viewport,
                ImageViewportInternal::DisplayRequestOrigin::Playback, primaryRequest.target,
                primaryRequest.resolvedFrame, false);
            const DisplayRequestTarget providerTarget {
                target.displayTarget.frame,
                target.displayTarget.position,
                ImageViewportInternal::ProviderRequestTargetKind::Playback,
            };
            setSecondaryActiveRequest(viewport, providerTarget,
                { providerTarget.frame,
                    state.secondaryProvider.timingIntervals.frameStartPosition(
                        providerTarget.frame) });
            viewportRequestState(viewport).status = ImageViewport::RequestStatus::Loading;
            viewportRequestState(viewport).reason = ImageViewport::RequestReason::ProviderWaiting;
            viewportDisplayState(viewport).status
                = viewportDisplayState(viewport).displayedImageSize.isValid()
                ? ImageViewport::DisplayStatus::Retained
                : ImageViewport::DisplayStatus::Empty;
            const ViewportProviderFrameRequestStartResult start
                = startSecondaryProviderFrameRequest(providerTarget);
            appendProviderFrameStartResult(result.secondaryProviderFrameTransport, start);
            if (start.accepted) {
                updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
            }

            applyPlaybackAdvancePhase(viewport, result.changes, target);
            result.changes.requestRevision = true;
            if (target.displayTarget.frame != previousFrame
                || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
                result.changes.displayRevision = true;
            }
            result.changes.requestState = true;
            result.changes.displayState = true;
            result.changes.scheduleUpdate = true;
            if (!start.accepted) {
                result.changes.diagnostics = true;
            }
            return result;
        }

        if (!viewport.hasSecondaryTimedSequence()) {
            return result;
        }

        const int totalDuration = viewport.secondaryTotalDuration();
        const int previousFrame
            = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
        const int currentFrame = viewportRequestState(viewport).secondaryActiveRequest.target.frame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
            effectiveLoopingForPlayback(
                viewport, viewport.secondarySequenceAuthoredAnimationFacts()),
            totalDuration, viewport.secondarySequenceFrameCount(),
            [this](int frame) { return viewport.secondarySequenceFrameStartPosition(frame); },
            [this](int position) {
                return viewport.secondarySequenceFrameIndexForPosition(position);
            });
        if (!target.valid) {
            return result;
        }

        viewportRequestState(viewport).playbackPosition = target.playbackPosition;
        if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
            return result;
        }

        const QRectF oldContentRect = viewport.contentRect();
        const QRectF oldVisibleImageRect = viewport.visibleImageRect();
        const ImageViewportInternal::DisplayRequest primaryRequest
            = viewportRequestState(viewport).activeRequest;
        beginAcceptedDisplayRequest(viewport, ImageViewportInternal::DisplayRequestOrigin::Playback,
            primaryRequest.target, primaryRequest.resolvedFrame, false);
        setSecondaryActiveRequest(viewport, target.displayTarget,
            { target.displayTarget.frame, target.displayTarget.position });
        publishAcceptedTargetState(viewport);
        updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
        applyPlaybackAdvancePhase(viewport, result.changes, target);
        result.changes.requestRevision = true;
        if (target.displayTarget.frame != previousFrame
            || viewportDisplayState(viewport).status != ImageViewport::DisplayStatus::Ready) {
            result.changes.displayRevision = true;
        }
        result.changes.requestState = true;
        result.changes.displayState = true;
        result.changes.geometryState
            = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
            || ImageViewportInternal::rectsDifferExactly(
                viewport.visibleImageRect(), oldVisibleImageRect);
        result.changes.scheduleUpdate = true;
        return result;
    }

    if (viewport.hasProviderSequence() && viewportProviderState(viewport).metadataReady
        && viewportProviderState(viewport).timedMetadata) {
        const int duration = viewport.totalDuration();
        const int previousFrame = viewportRequestState(viewport).activeRequest.target.frame;
        const int currentFrame = viewportRequestState(viewport).activeRequest.target.frame;
        const PlaybackAdvanceTarget target = playbackAdvanceTarget(
            elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
            effectiveLoopingForPlayback(viewport, viewport.providerAuthoredAnimationFacts()),
            duration, viewport.frameCount(),
            [this](int frame) { return viewport.providerFrameStartPosition(frame); },
            [this](int position) { return viewport.providerFrameIndexForPosition(position); });
        if (!target.valid) {
            return result;
        }

        viewportRequestState(viewport).playbackPosition = target.playbackPosition;
        if (target.displayTarget.frame == previousFrame
            && viewportRequestState(viewport).status == ImageViewport::RequestStatus::Ready) {
            if (viewportRequestState(viewport).stopPlaybackWhenRequestReady) {
                setPlaybackPhase(viewport, result.changes, ImageViewport::PlaybackPhase::Stopped);
                viewportRequestState(viewport).stopPlaybackWhenRequestReady = false;
            } else {
                applyPlaybackAdvancePhase(viewport, result.changes, target);
            }
            return result;
        }

        applyPlaybackTarget(viewport, target.displayTarget);
        viewportRequestState(viewport).activeRequest.target.providerTargetKind
            = ImageViewportInternal::ProviderRequestTargetKind::Playback;
        publishProviderFrameLoadingState(viewport);
        const bool diagnosticsValueChanged = viewportRequestState(viewport).clearDiagnostics();
        const ViewportProviderFrameDispatchResult dispatch
            = dispatchProviderFrameRequest({ viewportRequestState(viewport).activeRequest.target });
        result.providerFrameTransport = dispatch.transport;
        appendPlaybackRequestChange(viewport, result.changes, previousFrame);
        if (!dispatch.accepted) {
            result.changes.diagnostics = true;
            result.changes.scheduleUpdate = true;
            return result;
        }

        applyPlaybackAdvancePhase(viewport, result.changes, target);
        updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
        result.changes.diagnostics = diagnosticsValueChanged;
        result.changes.scheduleUpdate = true;
        return result;
    }

    if (!viewport.hasTimedSequence()) {
        return result;
    }

    const int totalDuration = viewport.totalDuration();
    const int previousFrame = viewportRequestState(viewport).activeRequest.target.frame;
    const int currentFrame = viewportRequestState(viewport).activeRequest.target.frame;
    const PlaybackAdvanceTarget target = playbackAdvanceTarget(
        elapsedMilliseconds, currentFrame, viewportRequestState(viewport).playbackPosition,
        effectiveLoopingForPlayback(viewport, viewport.sequenceAuthoredAnimationFacts()),
        totalDuration, viewport.sequenceFrameCount(),
        [this](int frame) { return viewport.sequenceFrameStartPosition(frame); },
        [this](int position) { return viewport.sequenceFrameIndexForPosition(position); });
    if (!target.valid) {
        return result;
    }

    viewportRequestState(viewport).playbackPosition = target.playbackPosition;
    if (!target.reachedEnd && !target.looped && target.displayTarget.frame == currentFrame) {
        return result;
    }

    applyPlaybackTarget(viewport, target.displayTarget);
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    publishAcceptedTargetState(viewport);
    updateLoopProgressForAcceptedPlaybackTarget(viewport, target);
    applyPlaybackAdvancePhase(viewport, result.changes, target);
    appendPlaybackRequestChange(viewport, result.changes, previousFrame);
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.scheduleUpdate = true;
    return result;
}
