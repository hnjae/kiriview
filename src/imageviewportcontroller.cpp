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

bool ImageViewportPrivate::hasActiveRequest() const
{
    return request.status != RequestStatus::NoRequest;
}

bool ImageViewportPrivate::hasReadyDisplay() const
{
    return display.hasReadyDisplay(hasDisplayableSequence());
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

QSizeF ImageViewportPrivate::sequenceLogicalSize() const
{
    return request.sequence ? request.sequence->logicalSize() : QSizeF {};
}

QImage ImageViewportPrivate::sequenceFrameImage(int frame) const
{
    return request.sequence ? request.sequence->frameImage(frame) : QImage();
}

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}
