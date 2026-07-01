#include "framepreparation_p.h"
#include "imageviewport_p.h"

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    const ViewportPlaybackAdvanceResult result = controller.advancePlayback(elapsedMilliseconds);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
}

void ImageViewportPrivate::incrementDisplayRevision()
{
    controller.incrementDisplayRevision();
    emit q->displayRevisionChanged();
}

void ImageViewportPrivate::incrementRequestRevision()
{
    controller.incrementRequestRevision();
    emit q->requestRevisionChanged();
}

void ImageViewportPrivate::syncPlaybackTimer()
{
    const int interval = controller.playbackTimerInterval();
    if (interval <= 0) {
        stopPlaybackTimer();
        return;
    }

    playbackClock.restart(playbackClockTimebase.elapsed());
    playbackTimer.start(interval);
}

void ImageViewportPrivate::stopPlaybackTimer()
{
    playbackTimer.stop();
    playbackClock.invalidate();
}

void ImageViewportPrivate::handlePlaybackTimer()
{
    advancePlayback(takePlaybackTimerElapsed());
    syncPlaybackTimer();
}

int ImageViewportPrivate::takePlaybackTimerElapsed()
{
    const int elapsedMilliseconds = playbackClock.takeElapsed(playbackClockTimebase.elapsed());
    playbackTimer.stop();
    return elapsedMilliseconds;
}

void ImageViewportPrivate::flushPlaybackTimerElapsed()
{
    if (!playbackClock.isValid()) {
        return;
    }

    advancePlayback(takePlaybackTimerElapsed());
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
    return hasProviderSequence() && !controller.hasProviderSession()
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
