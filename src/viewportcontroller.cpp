#include "viewportcontroller_p.h"

#include "imageviewport_p.h"

ViewportController::ViewportController(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
}

ImageViewport::CommandOutcome ViewportController::clear()
{
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
    viewport.clearCommandDiagnosticForAcceptedCommand();
    if (requestChanged) {
        viewport.incrementRequestRevision();
    }
    if (displayChanged) {
        viewport.incrementDisplayRevision();
    }

    if (sequenceValueChanged) {
        emit viewport.q->sequenceChanged();
    }
    if (requestChanged) {
        emit viewport.q->requestStateChanged();
    }
    if (displayChanged) {
        emit viewport.q->displayStateChanged();
    }
    if (ImageViewportInternal::rectsDifferExactly(viewport.contentRect(), oldContentRect)
        || ImageViewportInternal::rectsDifferExactly(
            viewport.visibleImageRect(), oldVisibleImageRect)) {
        emit viewport.q->geometryStateChanged();
    }
    if (playbackChanged) {
        emit viewport.q->playbackPhaseChanged();
    }
    if (diagnosticsValueChanged) {
        emit viewport.q->diagnosticsChanged();
    }
    viewport.update();
    return ImageViewport::CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ViewportController::play() { return viewport.playCommandImpl(); }

ImageViewport::CommandOutcome ViewportController::pause()
{
    if (!viewport.hasActiveRequest()) {
        return viewport.ignoredNoRequest();
    }

    viewport.clearCommandDiagnosticForAcceptedCommand();
    if (viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Playing
        || viewport.m_playbackPhase == ImageViewport::PlaybackPhase::Waiting) {
        viewport.setPlaybackPhase(ImageViewport::PlaybackPhase::Paused);
    }
    return ImageViewport::CommandOutcome::Accepted;
}

ImageViewport::CommandOutcome ViewportController::stop() { return viewport.stopCommandImpl(); }

ImageViewport::CommandOutcome ViewportController::seek(int frame)
{
    return viewport.seekCommandImpl(frame);
}

ImageViewport::CommandOutcome ViewportController::seekToPosition(int milliseconds)
{
    return viewport.seekToPositionCommandImpl(milliseconds);
}

ImageViewport::CommandOutcome ViewportController::resetView()
{
    const bool changed = viewport.m_zoom != 1.0 || viewport.m_pan.x() != 0.0
        || viewport.m_pan.y() != 0.0;
    viewport.m_zoom = 1.0;
    viewport.m_pan = {};
    if (changed) {
        viewport.notifyPresentationChanged(true);
    }
    viewport.clearCommandDiagnosticForAcceptedCommand();
    return ImageViewport::CommandOutcome::Accepted;
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

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.clear();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.play();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.pause();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.stop();
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.seek(frame);
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    flushPlaybackTimerElapsed();
    const ImageViewport::CommandOutcome outcome = controller.seekToPosition(milliseconds);
    syncPlaybackTimer();
    return outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView() { return controller.resetView(); }

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
