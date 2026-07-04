#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewportvalidation_p.h"
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
    return controller.requestState().sequenceSource.facts.present;
}

bool ImageViewportPrivate::hasStillSequence() const
{
    return sourceIsStill(controller.requestState().sequenceSource);
}

bool ImageViewportPrivate::hasTimedSequence() const
{
    return controller.requestState().sequenceSource.facts.timed;
}

bool ImageViewportPrivate::hasProviderSequence() const
{
    return controller.requestState().sequenceSource.facts.provider;
}

bool ImageViewportPrivate::hasGenerationTerminalProviderFailure() const
{
    return hasProviderSequence() && !controller.hasProviderSession()
        && (controller.requestState().status == RequestStatus::Unsupported
            || controller.requestState().status == RequestStatus::Error);
}

bool ImageViewportPrivate::providerTimedPlaybackCapabilityKnownFalse() const
{
    return providerCapabilityKnownFalse(
        controller.requestState().sequenceSource.facts.providerTimedPlaybackCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownFalse() const
{
    return providerCapabilityKnownFalse(
        controller.requestState().sequenceSource.facts.providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerFrameSeekCapabilityKnownTrue() const
{
    return providerCapabilityKnownTrue(
        controller.requestState().sequenceSource.facts.providerFrameSeekCapability);
}

bool ImageViewportPrivate::providerPositionSeekCapabilityKnownFalse() const
{
    return providerCapabilityKnownFalse(
        controller.requestState().sequenceSource.facts.providerPositionSeekCapability);
}

bool ImageViewportPrivate::providerKnownFactsTimedFrameCount() const
{
    return controller.requestState().sequenceSource.facts.providerKnownFacts.isTimedFrameCount();
}

int ImageViewportPrivate::providerKnownFactsFrameCount() const
{
    return providerKnownFactsTimedFrameCount()
        ? controller.requestState().sequenceSource.facts.providerKnownFacts.frameCount()
        : 0;
}

int ImageViewportPrivate::sequenceFrameCount() const
{
    return controller.requestState().sequenceSource.facts.frameCount;
}

int ImageViewportPrivate::sequenceTotalDuration() const
{
    return controller.requestState().sequenceSource.facts.totalDuration;
}

int ImageViewportPrivate::sequenceFrameIndexForPosition(int position) const
{
    return sourceFrameIndexForPosition(controller.requestState().sequenceSource, position);
}

int ImageViewportPrivate::sequenceFrameStartPosition(int frame) const
{
    return sourceFrameStartPosition(controller.requestState().sequenceSource, frame);
}

QSizeF ImageViewportPrivate::sequenceLogicalSize() const
{
    return sourceLogicalSize(controller.requestState().sequenceSource);
}

QSizeF ImageViewportPrivate::secondarySequenceLogicalSize() const
{
    return sourceLogicalSize(controller.requestState().secondarySequenceSource);
}

QImage ImageViewportPrivate::sequenceFrameImage(int frame) const
{
    return sourceFrameImage(controller.requestState().sequenceSource, frame);
}

QImage ImageViewportPrivate::secondarySequenceFrameImage(int frame) const
{
    return sourceFrameImage(controller.requestState().secondarySequenceSource, frame);
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
        const ViewportCommandResult result = controller.play(role);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.play(role);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
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
        const ViewportCommandResult result = controller.pause(role);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.pause(role);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
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
        const ViewportCommandResult result = controller.stop(role);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.stop(role);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
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
        const ViewportCommandResult result = controller.seek(role, frame);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seek(role, frame);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
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
        const ViewportCommandResult result = controller.seekToPosition(role, milliseconds);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    flushPlaybackTimerElapsed();
    const ViewportCommandResult result = controller.seekToPosition(role, milliseconds);
    applyProviderFrameTransportEffect(result.providerFrameTransport);
    applyProviderFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    syncPlaybackTimer();
    return result.outcome;
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

void ImageViewportPrivate::setNextProviderRequestTokenForTest(PageRole role, quint64 token)
{
    controller.setNextProviderRequestTokenForTest(role, token);
}

void ImageViewportPrivate::failNextProviderCommandDeliveryForTest(PageRole role)
{
    if (role == PageRole::Secondary) {
        secondaryProviderBridge.failNextCommandDeliveryForTest();
        return;
    }
    providerBridge.failNextCommandDeliveryForTest();
}

void ImageViewportPrivate::useSynchronousProviderExecutorForTest()
{
    ViewportProviderExecutor& executor = synchronousViewportProviderExecutorForTest();
    providerBridge.setExecutor(executor);
    secondaryProviderBridge.setExecutor(executor);
}

void ImageViewportPrivate::useSynchronousProviderQueueFlushSchedulerForTest()
{
    synchronousProviderQueueFlushScheduler = true;
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

quint64 ImageViewportPrivate::secondaryPendingRenderPayloadIdForTest() const
{
    return controller.secondaryPendingRenderPayloadIdForTest();
}

ImageViewportInternal::RenderFailureDiagnostic
ImageViewportPrivate::lastAcceptedRenderFailureDiagnosticForTest() const
{
    return controller.lastAcceptedRenderFailureDiagnosticForTest();
}

ImageViewportInternal::ProviderTransportDiagnostic
ImageViewportPrivate::lastProviderTransportDiagnosticForTest() const
{
    return lastProviderTransportDiagnostic;
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

void ImageViewportPrivate::acknowledgeRenderCommitForTest(quint64 generation, quint64 requestId,
    quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const ImageViewportInternal::PreparedPayloadIdentity primaryPayload {
        generation,
        requestId,
        primaryPreparedPayloadId,
    };
    const ImageViewportInternal::PreparedPayloadIdentity secondaryPayload {
        generation,
        requestId,
        secondaryPreparedPayloadId,
    };
    const auto changes = controller.acknowledgeRenderCommit(
        {
            primaryPayload,
            {
                { ImageViewport::PageRole::Primary, primaryPayload },
                { ImageViewport::PageRole::Secondary, secondaryPayload },
            },
        },
        true, synchronization);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}

void ImageViewportPrivate::acknowledgeRenderFailureForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    acknowledgeRenderFailureForTest(
        PageRole::Primary, generation, requestId, preparedPayloadId, RenderFailureCause::None);
}

void ImageViewportPrivate::acknowledgeRenderFailureForTest(
    PageRole failedRole, quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    acknowledgeRenderFailureForTest(
        failedRole, generation, requestId, preparedPayloadId, RenderFailureCause::None);
}

void ImageViewportPrivate::acknowledgeRenderFailureForTest(PageRole failedRole, quint64 generation,
    quint64 requestId, quint64 preparedPayloadId, RenderFailureCause cause)
{
    const ImageViewportInternal::PreparedPayloadIdentity failedPayload {
        generation,
        requestId,
        preparedPayloadId,
    };
    const auto changes = controller.acknowledgeRenderFailure(
        { failedPayload, { { failedRole, failedPayload } }, failedRole, cause });
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        syncPlaybackTimer();
    }
}
#endif
