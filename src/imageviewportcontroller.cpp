#include "framepreparation_p.h"
#include "imageviewport_p.h"

#include <algorithm>
#include <limits>

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    const ViewportPlaybackAdvanceResult result = controller.advancePlayback(elapsedMilliseconds);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTestImpl(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
    syncPlaybackTimer();
}

void ImageViewportPrivate::setNextProviderRequestTokenForTestImpl(quint64 token)
{
    provider.nextRequestToken = token;
}

bool ImageViewportPrivate::hasPendingRenderCommitForTestImpl() const
{
    return display.pendingRenderPayload.commitPending;
}

quint64 ImageViewportPrivate::activeRequestIdForTestImpl() const
{
    return request.activeRequest.identity.id;
}

quint64 ImageViewportPrivate::displayedRequestIdForTestImpl() const
{
    return display.displayedRequest.request.identity.id;
}

quint64 ImageViewportPrivate::pendingRenderGenerationForTestImpl() const
{
    return display.pendingRenderPayload.generation;
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTestImpl() const
{
    return display.pendingRenderPayload.payloadId;
}
#endif

void ImageViewportPrivate::incrementDisplayRevision()
{
    ++display.revision;
    emit q->displayRevisionChanged();
}

void ImageViewportPrivate::incrementRequestRevision()
{
    ++request.requestRevision;
    emit q->requestRevisionChanged();
}

void ImageViewportPrivate::setPlaybackPhase(PlaybackPhase phase)
{
    if (request.playbackPhase == phase) {
        return;
    }

    request.playbackPhase = phase;
    emit q->playbackPhaseChanged();
    syncPlaybackTimer();
}

void ImageViewportPrivate::syncPlaybackTimer()
{
    const int interval = playbackTimerInterval();
    if (interval <= 0) {
        stopPlaybackTimer();
        return;
    }

    playbackElapsedTimer.restart();
    playbackTimer.start(interval);
}

void ImageViewportPrivate::stopPlaybackTimer()
{
    playbackTimer.stop();
    playbackElapsedTimer.invalidate();
}

void ImageViewportPrivate::handlePlaybackTimer()
{
    advancePlayback(takePlaybackTimerElapsed());
    syncPlaybackTimer();
}

int ImageViewportPrivate::takePlaybackTimerElapsed()
{
    const qint64 elapsedMilliseconds
        = playbackElapsedTimer.isValid() ? playbackElapsedTimer.elapsed() : 0;
    playbackTimer.stop();
    playbackElapsedTimer.invalidate();
    return static_cast<int>(std::min<qint64>(elapsedMilliseconds, std::numeric_limits<int>::max()));
}

void ImageViewportPrivate::flushPlaybackTimerElapsed()
{
    if (!playbackElapsedTimer.isValid()) {
        return;
    }

    advancePlayback(takePlaybackTimerElapsed());
}

int ImageViewportPrivate::playbackTimerInterval() const
{
    if (request.playbackPhase != PlaybackPhase::Playing || request.status != RequestStatus::Ready) {
        return -1;
    }

    int frameStart = -1;
    int frameDuration = -1;
    const int currentFrame = request.activeRequest.target.frame;
    if (hasProviderSequence() && provider.metadataReady && provider.timedMetadata) {
        if (currentFrame < 0 || currentFrame >= provider.timingIntervals.frameCount()) {
            return -1;
        }
        frameStart = providerFrameStartPosition(currentFrame);
        frameDuration = provider.timingIntervals.frameDuration(currentFrame);
    } else if (hasTimedSequence()) {
        if (currentFrame < 0 || currentFrame >= request.sequence->frameCount()) {
            return -1;
        }
        frameStart = request.sequence->frameStartPosition(currentFrame);
        const int nextFrameStart = currentFrame + 1 < request.sequence->frameCount()
            ? request.sequence->frameStartPosition(currentFrame + 1)
            : request.sequence->totalDuration();
        frameDuration = nextFrameStart - frameStart;
    } else {
        return -1;
    }

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition
        = request.playbackPosition >= 0 ? request.playbackPosition : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

void ImageViewportPrivate::setCommandDiagnostic(CommandReason reason)
{
    request.commandReason = reason;
    ++request.commandRevision;
    emit q->commandRevisionChanged();
    emit q->commandStateChanged();
}

void ImageViewportPrivate::clearCommandDiagnosticForAcceptedCommand()
{
    if (request.commandReason == CommandReason::NoCommand) {
        return;
    }

    setCommandDiagnostic(CommandReason::NoCommand);
}

bool ImageViewportPrivate::clearDiagnostics()
{
    if (request.errorString.isEmpty() && request.warningString.isEmpty()) {
        return false;
    }

    request.errorString.clear();
    request.warningString.clear();
    return true;
}

void ImageViewportPrivate::clearRequestIdentity()
{
    request.nextRequestId = 0;
    request.activeRequest.identity = {};
    request.latestNonPlaybackRequest.identity = {};
}

void ImageViewportPrivate::beginDisplayRequest(
    DisplayRequestOrigin origin, bool rememberAsLatestNonPlayback)
{
    request.activeRequest.identity.id = ++request.nextRequestId;
    request.activeRequest.identity.origin = origin;
    request.activeRequest.providerFrameToken = {};
    request.activeRequest.preparedPayloadId = 0;
    if (rememberAsLatestNonPlayback) {
        request.latestNonPlaybackRequest.identity = request.activeRequest.identity;
    }
}

void ImageViewportPrivate::beginInitialDisplayRequest(
    DisplayRequestTarget target, bool rememberAsLatestNonPlayback)
{
    beginDisplayRequest(DisplayRequestOrigin::Initial, rememberAsLatestNonPlayback);
    request.activeRequest.target = target;
    if (rememberAsLatestNonPlayback) {
        request.latestNonPlaybackRequest.target = target;
    }
}

ImageViewportPrivate::DisplayRequestSnapshot ImageViewportPrivate::activeDisplayRequestSnapshot(
    int displayedPosition) const
{
    DisplayRequestSnapshot snapshot = display.displayedRequest;
    snapshot.generation = request.sequenceGeneration;
    snapshot.request.target = request.activeRequest.target;
    snapshot.request.target.frame = request.activeRequest.target.frame;
    snapshot.request.target.position = displayedPosition;
    return snapshot;
}

void ImageViewportPrivate::commitDisplayedRequestSnapshot()
{
    const auto displayedTarget = display.displayedRequest.request.target;
    display.displayedRequest.generation = request.sequenceGeneration;
    display.displayedRequest.request = request.activeRequest;
    display.displayedRequest.request.target = displayedTarget;
    display.displayedRequest.request.preparedPayloadId = display.pendingRenderPayload.payloadId;
}

void ImageViewportPrivate::clearDisplayedDisplay()
{
    display.displayedRequest = {};
    display.displayedImageSize = {};
    display.displayedImage = {};
}

void ImageViewportPrivate::beginPreparedPayloadIdentity()
{
    display.pendingRenderPayload.generation = request.sequenceGeneration;
    display.pendingRenderPayload.requestId = request.activeRequest.identity.id;
    display.pendingRenderPayload.payloadId
        = request.activeRequest.identity.id == 0 ? 0 : ++display.nextPreparedPayloadId;
    request.activeRequest.preparedPayloadId = display.pendingRenderPayload.payloadId;
}

void commitPreparedPayloadIdentity(
    ImageViewportPrivate& viewport, const ImageViewportInternal::PreparedPayload& preparedPayload)
{
    viewport.display.pendingRenderPayload = preparedPayload;
    viewport.request.activeRequest.preparedPayloadId = preparedPayload.payloadId;
    if (preparedPayload.payloadId > viewport.display.nextPreparedPayloadId) {
        viewport.display.nextPreparedPayloadId = preparedPayload.payloadId;
    }
}

void ImageViewportPrivate::clearPendingRenderIdentity() { display.pendingRenderPayload = {}; }

ImageViewportPrivate::CommandOutcome ImageViewportPrivate::ignoredNoRequest()
{
    setCommandDiagnostic(CommandReason::IgnoredNoRequest);
    return CommandOutcome::IgnoredNoRequest;
}

bool ImageViewportPrivate::hasActiveRequest() const
{
    return request.status != RequestStatus::NoRequest;
}

bool ImageViewportPrivate::hasReadyDisplay() const
{
    return hasDisplayableSequence()
        && (display.status == DisplayStatus::Ready || display.status == DisplayStatus::Retained)
        && display.displayedImageSize.isValid() && display.displayedImageSize.width() > 0.0
        && display.displayedImageSize.height() > 0.0;
}

bool ImageViewportPrivate::hasDisplayableSequence() const
{
    return request.sequence && request.sequence->isValid();
}

bool ImageViewportPrivate::hasStillSequence() const
{
    return request.sequence && request.sequence->isStill();
}

bool ImageViewportPrivate::hasTimedSequence() const
{
    return request.sequence && request.sequence->isTimedList();
}

bool ImageViewportPrivate::hasProviderSequence() const
{
    return request.sequence && request.sequence->isProvider();
}

bool ImageViewportPrivate::hasGenerationTerminalProviderFailure() const
{
    return hasProviderSequence() && !provider.session
        && (request.status == RequestStatus::Unsupported || request.status == RequestStatus::Error);
}

bool ImageViewportPrivate::providerTimedPlaybackCapabilityKnownFalse() const
{
    return request.sequence
        && providerCapabilityKnownFalse(request.sequence->m_providerTimedPlaybackCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownFalse() const
{
    return request.sequence
        && providerCapabilityKnownFalse(request.sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownTrue() const
{
    return request.sequence
        && providerCapabilityKnownTrue(request.sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerPositionSeekCapabilityKnownFalse() const
{
    return request.sequence
        && providerCapabilityKnownFalse(request.sequence->m_providerPositionSeekCapability);
}

bool ImageViewportPrivate::providerKnownFactsTimedFrameCount() const
{
    return request.sequence && request.sequence->m_providerKnownFacts.isTimedFrameCount();
}

int ImageViewportPrivate::providerKnownFactsFrameCount() const
{
    return providerKnownFactsTimedFrameCount() ? request.sequence->m_providerKnownFacts.frameCount()
                                               : 0;
}

int ImageViewportPrivate::sequenceFrameCount() const
{
    return request.sequence ? request.sequence->frameCount() : 0;
}

int ImageViewportPrivate::sequenceFrameIndexForPosition(int position) const
{
    return request.sequence ? request.sequence->frameIndexForPosition(position) : -1;
}

int ImageViewportPrivate::sequenceFrameStartPosition(int frame) const
{
    return request.sequence ? request.sequence->frameStartPosition(frame) : -1;
}

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}
void ImageViewportPrivate::publishAcceptedTargetState(const QImage& providerImage)
{
    if (hasProviderSequence() && !providerImage.isNull()) {
        captureRenderFailureRetainedDisplay();
        display.pendingRenderPayload.image = providerImage;
        beginPreparedPayloadIdentity();
        if (itemBounds().isEmpty()) {
            publishRenderWaitingState();
        } else {
            request.status = RequestStatus::Loading;
            request.reason = RequestReason::UploadPending;
            display.status = display.displayedImageSize.isValid() ? DisplayStatus::Retained
                                                                  : DisplayStatus::Empty;
            display.pendingRenderPayload.commitPending = false;
        }
        display.pendingRenderPayload.commitPending = true;
        return;
    }
    if (itemBounds().isEmpty()) {
        publishRenderWaitingState();
    } else {
        publishSequenceReadyState(providerImage);
    }
}

void ImageViewportPrivate::publishAcceptedTargetState(
    const ImageViewportInternal::PreparedPayload& providerPayload)
{
    if (hasProviderSequence() && !providerPayload.image.isNull()) {
        captureRenderFailureRetainedDisplay();
        commitPreparedPayloadIdentity(*this, providerPayload);
        if (itemBounds().isEmpty()) {
            publishRenderWaitingState();
        } else {
            request.status = RequestStatus::Loading;
            request.reason = RequestReason::UploadPending;
            display.status = display.displayedImageSize.isValid() ? DisplayStatus::Retained
                                                                  : DisplayStatus::Empty;
            display.pendingRenderPayload.commitPending = false;
        }
        display.pendingRenderPayload.commitPending = true;
        return;
    }
    publishAcceptedTargetState(providerPayload.image);
}

void ImageViewportPrivate::publishReadyDisplayState()
{
    request.status = RequestStatus::Ready;
    request.reason = RequestReason::Ready;
    display.status = DisplayStatus::Ready;
}

void ImageViewportPrivate::publishSequenceReadyState(const QImage& providerImage)
{
    captureRenderFailureRetainedDisplay();
    publishReadyDisplayState();
    display.pendingRenderPayload.commitPending = true;
    beginPreparedPayloadIdentity();
    int displayedPosition = -1;
    const int currentFrame = request.activeRequest.target.frame;
    if (hasProviderSequence()) {
        displayedPosition = providerFrameStartPosition(currentFrame);
    } else {
        displayedPosition
            = hasTimedSequence() ? request.sequence->frameStartPosition(currentFrame) : -1;
    }
    display.displayedRequest = activeDisplayRequestSnapshot(displayedPosition);
    display.displayedImageSize
        = hasProviderSequence() ? provider.logicalSize : request.sequence->logicalSize();
    if (hasProviderSequence()) {
        if (!providerImage.isNull()) {
            display.displayedImage = providerImage;
        } else if (!display.pendingRenderPayload.image.isNull()) {
            display.displayedImage = display.pendingRenderPayload.image;
        }
        display.pendingRenderPayload.image = {};
    } else {
        display.displayedImage = request.sequence
            ? request.sequence->frameImage(display.displayedRequest.request.target.frame)
            : QImage();
    }
}

void ImageViewportPrivate::publishSequenceReadyState(
    const ImageViewportInternal::PreparedPayload& providerPayload)
{
    if (!hasProviderSequence() || providerPayload.image.isNull()) {
        publishSequenceReadyState(providerPayload.image);
        return;
    }

    captureRenderFailureRetainedDisplay();
    publishReadyDisplayState();
    commitPreparedPayloadIdentity(*this, providerPayload);
    display.pendingRenderPayload.commitPending = true;
    const int currentFrame = request.activeRequest.target.frame;
    display.displayedRequest
        = activeDisplayRequestSnapshot(providerFrameStartPosition(currentFrame));
    display.displayedImageSize = provider.logicalSize;
    display.displayedImage = providerPayload.image;
    display.pendingRenderPayload.image = {};
}

void ImageViewportPrivate::publishRenderWaitingState()
{
    request.status = RequestStatus::Loading;
    request.reason = RequestReason::RenderWaiting;
    display.status
        = display.displayedImageSize.isValid() ? DisplayStatus::Retained : DisplayStatus::Empty;
    display.pendingRenderPayload.commitPending = false;
}
