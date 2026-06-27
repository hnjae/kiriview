#include "viewportcontroller_p.h"

#include "imageviewport_p.h"

namespace {
void setCommandDiagnostic(ImageViewportPrivate& viewport, ViewportCommandResult& result,
    ImageViewport::CommandReason reason)
{
    viewport.m_commandReason = reason;
    result.changes.commandRevision = true;
}

void clearCommandDiagnosticForAcceptedCommand(
    ImageViewportPrivate& viewport, ViewportCommandResult& result)
{
    if (viewport.m_commandReason == ImageViewport::CommandReason::NoCommand) {
        return;
    }

    setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::NoCommand);
}
}

ViewportController::ViewportController(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
}

ViewportCommandResult ViewportController::clear()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const bool sequenceValueChanged = viewport.m_sequence != nullptr;
    const bool requestChanged = viewport.hasActiveRequest() || viewport.m_sequence;
    const bool displayChanged = viewport.m_displayStatus != ImageViewport::DisplayStatus::Empty
        || viewport.m_displayedImageSize.isValid();
    const bool playbackChanged
        = viewport.m_playbackPhase != ImageViewport::PlaybackPhase::Stopped;
    const bool diagnosticsValueChanged
        = !viewport.m_errorString.isEmpty() || !viewport.m_warningString.isEmpty();
    const QRectF oldContentRect = viewport.contentRect();
    const QRectF oldVisibleImageRect = viewport.visibleImageRect();
    viewport.closeProviderSession();
    viewport.m_sequence = nullptr;
    viewport.m_sequenceOwner.reset();
    ++viewport.m_sequenceGeneration;
    viewport.clearRequestIdentity();
    viewport.m_currentFrame = -1;
    viewport.m_requestedPosition = -1;
    viewport.m_playbackPosition = -1;
    viewport.m_latestNonPlaybackFrame = -1;
    viewport.m_latestNonPlaybackPosition = -1;
    viewport.m_currentProviderTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_latestNonPlaybackProviderTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_displayedFrame = -1;
    viewport.m_displayedPosition = -1;
    viewport.m_displayedGeneration = 0;
    viewport.m_displayedRequestId = 0;
    viewport.m_displayedPreparedPayloadId = 0;
    viewport.m_displayedImageSize = {};
    viewport.m_displayedImage = {};
    viewport.m_pendingDisplayImage = {};
    viewport.m_renderCommitPending = false;
    viewport.m_nextPreparedPayloadId = 0;
    viewport.clearPendingRenderIdentity();
    viewport.clearRenderFailureRetainedDisplay();
    viewport.m_requestStatus = ImageViewport::RequestStatus::NoRequest;
    viewport.m_requestReason = ImageViewport::RequestReason::NoRequest;
    viewport.m_displayStatus = ImageViewport::DisplayStatus::Empty;
    viewport.m_playbackPhase = ImageViewport::PlaybackPhase::Stopped;
    viewport.m_stopPlaybackWhenRequestReady = false;
    viewport.m_providerPlaybackStartPending = false;
    viewport.m_providerMetadataReady = false;
    viewport.m_providerTimedMetadata = false;
    viewport.m_providerLogicalSize = {};
    viewport.m_providerTimingIntervals = {};
    viewport.m_activeProviderMetadataToken = {};
    viewport.m_activeProviderFrameToken = {};
    viewport.m_activeProviderFrameRequestId = 0;
    viewport.m_activeProviderFrameFromPlayback = false;
    viewport.m_activeProviderFrameTargetKind
        = ImageViewportInternal::ProviderRequestTargetKind::Unknown;
    viewport.m_errorString.clear();
    viewport.m_warningString.clear();
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    result.changes.requestRevision = requestChanged;
    result.changes.displayRevision = displayChanged;
    result.changes.sequence = sequenceValueChanged;
    result.changes.requestState = requestChanged;
    result.changes.displayState = displayChanged;
    result.changes.geometryState
        = ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect);
    result.changes.playbackPhase = playbackChanged;
    result.changes.diagnostics = diagnosticsValueChanged;
    result.changes.scheduleUpdate = true;
    return result;
}

ViewportCommandResult ViewportController::play() { return { viewport.playCommandImpl(), {} }; }

ViewportCommandResult ViewportController::pause()
{
    if (!viewport.hasActiveRequest()) {
        ViewportCommandResult result;
        result.outcome = ImageViewport::CommandOutcome::IgnoredNoRequest;
        setCommandDiagnostic(viewport, result, ImageViewport::CommandReason::IgnoredNoRequest);
        return result;
    }

    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewport.m_playbackPhase = ImageViewport::PlaybackPhase::Paused;
        result.changes.playbackPhase = true;
    }
    return result;
}

ViewportCommandResult ViewportController::stop() { return { viewport.stopCommandImpl(), {} }; }

ViewportCommandResult ViewportController::seek(int frame)
{
    return { viewport.seekCommandImpl(frame), {} };
}

ViewportCommandResult ViewportController::seekToPosition(int milliseconds)
{
    return { viewport.seekToPositionCommandImpl(milliseconds), {} };
}

ViewportCommandResult ViewportController::resetView()
{
    ViewportCommandResult result;
    result.outcome = ImageViewport::CommandOutcome::Accepted;
    const bool changed = viewport.m_zoom != 1.0 || viewport.m_pan.x() != 0.0
        || viewport.m_pan.y() != 0.0;
    viewport.m_zoom = 1.0;
    viewport.m_pan = {};
    if (changed) {
        result.changes.presentation = true;
        result.changes.displayRevision = true;
        result.changes.geometryState = viewport.hasReadyDisplay() && !viewport.itemBounds().isEmpty();
        result.changes.scheduleUpdate = true;
    }
    clearCommandDiagnosticForAcceptedCommand(viewport, result);
    return result;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportController::advancePlaybackForTest(int elapsedMilliseconds)
{
    viewport.advancePlaybackForTestImpl(elapsedMilliseconds);
}

void ViewportController::setNextProviderRequestTokenForTest(quint64 token)
{
    viewport.setNextProviderRequestTokenForTestImpl(token);
}

bool ViewportController::hasPendingRenderCommitForTest() const
{
    return viewport.hasPendingRenderCommitForTestImpl();
}

quint64 ViewportController::activeRequestIdForTest() const
{
    return viewport.activeRequestIdForTestImpl();
}

quint64 ViewportController::displayedRequestIdForTest() const
{
    return viewport.displayedRequestIdForTestImpl();
}

quint64 ViewportController::pendingRenderPayloadIdForTest() const
{
    return viewport.pendingRenderPayloadIdForTestImpl();
}
#endif

void ImageViewportPrivate::applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes)
{
    if (changes.displayRevision) {
        incrementDisplayRevision();
    }
    if (changes.requestRevision) {
        incrementRequestRevision();
    }
    if (changes.commandRevision) {
        ++m_commandRevision;
        emit q->commandRevisionChanged();
        emit q->commandStateChanged();
    }

    if (changes.sequence) {
        emit q->sequenceChanged();
    }
    if (changes.requestState) {
        emit q->requestStateChanged();
    }
    if (changes.displayState) {
        emit q->displayStateChanged();
    }
    if (changes.geometryState) {
        emit q->geometryStateChanged();
    }
    if (changes.playbackPhase) {
        emit q->playbackPhaseChanged();
    }
    if (changes.diagnostics) {
        emit q->diagnosticsChanged();
    }
    if (changes.presentation) {
        emit q->presentationChanged();
    }
    if (changes.scheduleUpdate) {
        update();
    }
}

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.clear();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.play();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.pause();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.stop();
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seek(frame);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seekToPosition(milliseconds);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    const ViewportCommandResult result = controller.resetView();
    applyControllerChanges(result.changes);
    return result.outcome;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    controller.advancePlaybackForTest(elapsedMilliseconds);
}

void ImageViewportPrivate::setNextProviderRequestTokenForTest(quint64 token)
{
    controller.setNextProviderRequestTokenForTest(token);
}

bool ImageViewportPrivate::hasPendingRenderCommitForTest() const
{
    return controller.hasPendingRenderCommitForTest();
}

quint64 ImageViewportPrivate::activeRequestIdForTest() const
{
    return controller.activeRequestIdForTest();
}

quint64 ImageViewportPrivate::displayedRequestIdForTest() const
{
    return controller.displayedRequestIdForTest();
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTest() const
{
    return controller.pendingRenderPayloadIdForTest();
}
#endif
