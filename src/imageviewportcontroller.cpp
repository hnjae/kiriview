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

void ImageViewportPrivate::incrementDisplayRevision()
{
    ++controller.displayState().revision;
    emit q->displayRevisionChanged();
}

void ImageViewportPrivate::incrementRequestRevision()
{
    ++controller.requestState().requestRevision;
    emit q->requestRevisionChanged();
}

void ImageViewportPrivate::setPlaybackPhase(PlaybackPhase phase)
{
    if (controller.requestState().playbackPhase == phase) {
        return;
    }

    controller.requestState().playbackPhase = phase;
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
    if (controller.requestState().playbackPhase != PlaybackPhase::Playing
        || controller.requestState().status != RequestStatus::Ready) {
        return -1;
    }

    int frameStart = -1;
    int frameDuration = -1;
    const int currentFrame = controller.requestState().activeRequest.target.frame;
    if (hasProviderSequence() && controller.providerState().metadataReady
        && controller.providerState().timedMetadata) {
        if (currentFrame < 0
            || currentFrame >= controller.providerState().timingIntervals.frameCount()) {
            return -1;
        }
        frameStart = providerFrameStartPosition(currentFrame);
        frameDuration = controller.providerState().timingIntervals.frameDuration(currentFrame);
    } else if (hasTimedSequence()) {
        if (currentFrame < 0 || currentFrame >= controller.requestState().sequence->frameCount()) {
            return -1;
        }
        frameStart = controller.requestState().sequence->frameStartPosition(currentFrame);
        const int nextFrameStart
            = currentFrame + 1 < controller.requestState().sequence->frameCount()
            ? controller.requestState().sequence->frameStartPosition(currentFrame + 1)
            : controller.requestState().sequence->totalDuration();
        frameDuration = nextFrameStart - frameStart;
    } else {
        return -1;
    }

    if (frameStart < 0 || frameDuration <= 0) {
        return -1;
    }

    const int playbackPosition = controller.requestState().playbackPosition >= 0
        ? controller.requestState().playbackPosition
        : frameStart;
    const int remaining = frameStart + frameDuration - playbackPosition;
    return std::max(1, remaining);
}

bool ImageViewportPrivate::hasActiveRequest() const
{
    return controller.requestState().status != RequestStatus::NoRequest;
}

bool ImageViewportPrivate::hasReadyDisplay() const
{
    return controller.displayState().hasReadyDisplay(hasDisplayableSequence());
}

bool ImageViewportPrivate::hasDisplayableSequence() const
{
    return controller.requestState().sequence && controller.requestState().sequence->isValid();
}

bool ImageViewportPrivate::hasStillSequence() const
{
    return controller.requestState().sequence && controller.requestState().sequence->isStill();
}

bool ImageViewportPrivate::hasTimedSequence() const
{
    return controller.requestState().sequence && controller.requestState().sequence->isTimedList();
}

bool ImageViewportPrivate::hasProviderSequence() const
{
    return controller.requestState().sequence && controller.requestState().sequence->isProvider();
}

bool ImageViewportPrivate::hasGenerationTerminalProviderFailure() const
{
    return hasProviderSequence() && !controller.providerState().session
        && (controller.requestState().status == RequestStatus::Unsupported
            || controller.requestState().status == RequestStatus::Error);
}

bool ImageViewportPrivate::providerTimedPlaybackCapabilityKnownFalse() const
{
    return controller.requestState().sequence
        && providerCapabilityKnownFalse(
            controller.requestState().sequence->m_providerTimedPlaybackCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownFalse() const
{
    return controller.requestState().sequence
        && providerCapabilityKnownFalse(
            controller.requestState().sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownTrue() const
{
    return controller.requestState().sequence
        && providerCapabilityKnownTrue(
            controller.requestState().sequence->m_providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerPositionSeekCapabilityKnownFalse() const
{
    return controller.requestState().sequence
        && providerCapabilityKnownFalse(
            controller.requestState().sequence->m_providerPositionSeekCapability);
}

bool ImageViewportPrivate::providerKnownFactsTimedFrameCount() const
{
    return controller.requestState().sequence
        && controller.requestState().sequence->m_providerKnownFacts.isTimedFrameCount();
}

int ImageViewportPrivate::providerKnownFactsFrameCount() const
{
    return providerKnownFactsTimedFrameCount()
        ? controller.requestState().sequence->m_providerKnownFacts.frameCount()
        : 0;
}

int ImageViewportPrivate::sequenceFrameCount() const
{
    return controller.requestState().sequence ? controller.requestState().sequence->frameCount()
                                              : 0;
}

int ImageViewportPrivate::sequenceFrameIndexForPosition(int position) const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->frameIndexForPosition(position)
        : -1;
}

int ImageViewportPrivate::sequenceFrameStartPosition(int frame) const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->frameStartPosition(frame)
        : -1;
}

QSizeF ImageViewportPrivate::sequenceLogicalSize() const
{
    return controller.requestState().sequence ? controller.requestState().sequence->logicalSize()
                                              : QSizeF {};
}

QImage ImageViewportPrivate::sequenceFrameImage(int frame) const
{
    return controller.requestState().sequence
        ? controller.requestState().sequence->frameImage(frame)
        : QImage();
}

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}
