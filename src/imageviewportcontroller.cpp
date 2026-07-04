#include "framepreparation_p.h"
#include "imageviewport_p.h"

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    const ViewportPlaybackAdvanceResult result = controller.advancePlayback(elapsedMilliseconds);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
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

void ImageViewportPrivate::applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes)
{
    if (changes.displayRevision) {
        incrementDisplayRevision();
    }
    if (changes.requestRevision) {
        incrementRequestRevision();
    }
    if (changes.commandRevision) {
        controller.incrementCommandRevision();
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
    if (changes.looping) {
        emit q->loopingChanged();
    }
    if (changes.scheduleUpdate) {
        update();
    }
}

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.clear();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.play();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play(PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return CommandOutcome::Invalid;
    }
    if (role == PageRole::Secondary) {
        ImageSequence* sequence = secondarySequence();
        if (!sequence || !sequence->isValid()) {
            return CommandOutcome::IgnoredNoRequest;
        }
        if (!sequence->isProvider() && !sequence->isTimedList()) {
            const ViewportCommandResult result = controller.rejectUnsupportedCommand();
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        if (!sequence->isProvider() && sequence->isTimedList()) {
            flushPlaybackTimerElapsed();
            const ViewportCommandResult result = controller.playSecondaryBuiltIn();
            applyControllerChanges(result.changes);
            syncPlaybackTimer();
            return result.outcome;
        }
        if (sequence->isProvider()) {
            flushPlaybackTimerElapsed();
            const ViewportCommandResult result = controller.playSecondaryProvider();
            applyControllerChanges(result.changes);
            syncPlaybackTimer();
            return result.outcome;
        }
        return CommandOutcome::IgnoredNoRequest;
    }

    return play();
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.pause();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause(PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return CommandOutcome::Invalid;
    }
    if (role == PageRole::Secondary) {
        ImageSequence* sequence = secondarySequence();
        if (!sequence || !sequence->isValid()) {
            return CommandOutcome::IgnoredNoRequest;
        }
        flushPlaybackTimerElapsed();
        const ViewportCommandResult result = controller.pause(PageRole::Secondary);
        applyControllerChanges(result.changes);
        syncPlaybackTimer();
        return result.outcome;
    }

    return pause();
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.stop();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop(PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return CommandOutcome::Invalid;
    }
    if (role == PageRole::Secondary) {
        ImageSequence* sequence = secondarySequence();
        if (!sequence || !sequence->isValid()) {
            return CommandOutcome::IgnoredNoRequest;
        }
        flushPlaybackTimerElapsed();
        const ViewportCommandResult result = controller.stop(PageRole::Secondary);
        applyProviderFrameTransportEffect(result.providerFrameTransport, PageRole::Secondary);
        applyControllerChanges(result.changes);
        syncPlaybackTimer();
        return result.outcome;
    }

    return stop();
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seek(frame);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(PageRole role, int frame)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return CommandOutcome::Invalid;
    }
    if (role == PageRole::Secondary) {
        ImageSequence* sequence = secondarySequence();
        if (!sequence || !sequence->isValid()) {
            return CommandOutcome::IgnoredNoRequest;
        }
        if (frame < 0) {
            const ViewportCommandResult result = controller.rejectInvalidCommand();
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        if (!sequence->isProvider() && frame >= sequence->frameCount()) {
            const ViewportCommandResult result = controller.rejectInvalidCommand();
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        if (!sequence->isProvider()) {
            const int position = sequence->isTimedList() ? sequence->frameStartPosition(frame) : -1;
            const ViewportCommandResult result = controller.seekSecondaryBuiltIn(
                { frame, position, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
                { frame, position });
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        return CommandOutcome::IgnoredNoRequest;
    }

    return seek(frame);
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seekToPosition(milliseconds);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(PageRole role, int milliseconds)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        return CommandOutcome::Invalid;
    }
    if (role == PageRole::Secondary) {
        ImageSequence* sequence = secondarySequence();
        if (!sequence || !sequence->isValid()) {
            return CommandOutcome::IgnoredNoRequest;
        }
        if (milliseconds < 0) {
            const ViewportCommandResult result = controller.rejectInvalidCommand();
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        if (!sequence->isProvider() && !sequence->isTimedList()) {
            const ViewportCommandResult result = controller.rejectUnsupportedCommand();
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        if (!sequence->isProvider() && sequence->isTimedList()
            && milliseconds > sequence->totalDuration()) {
            const ViewportCommandResult result = controller.rejectInvalidCommand();
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        if (!sequence->isProvider() && sequence->isTimedList()) {
            const int frame = sequence->frameIndexForPosition(milliseconds);
            const int position = sequence->frameStartPosition(frame);
            const ViewportCommandResult result = controller.seekSecondaryBuiltIn(
                { frame, milliseconds, ImageViewportInternal::ProviderRequestTargetKind::Unknown },
                { frame, position });
            applyControllerChanges(result.changes);
            return result.outcome;
        }
        return CommandOutcome::IgnoredNoRequest;
    }

    return seekToPosition(milliseconds);
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    const ViewportCommandResult result = controller.resetView();
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    return result.outcome;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
    syncPlaybackTimer();
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

quint64 ImageViewportPrivate::pendingRenderGenerationForTest() const
{
    return controller.pendingRenderGenerationForTest();
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTest() const
{
    return controller.pendingRenderPayloadIdForTest();
}

void ImageViewportPrivate::acknowledgeRenderCommitForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const auto changes = controller.acknowledgeRenderCommit(
        { { generation, requestId, preparedPayloadId } }, true, synchronization);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::acknowledgeRenderFailureForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    const auto changes
        = controller.acknowledgeRenderFailure({ { generation, requestId, preparedPayloadId } });
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}
#endif
