#include "framepreparation_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewportvalidation_p.h"
#include "imageviewport_p.h"

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    const ViewportPlaybackAdvanceResult result = controller.advancePlayback(elapsedMilliseconds);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
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

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}

void ImageViewportPrivate::applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes)
{
    internalDiagnostics.recordRenderFailure(changes.renderFailureDiagnostic);

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
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.clear();
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play()
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.play();
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play(PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        const ViewportCommandResult result = controller.play(role);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.play(role);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause()
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.pause();
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause(PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        const ViewportCommandResult result = controller.pause(role);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.pause(role);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop()
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.stop();
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop(PageRole role)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        const ViewportCommandResult result = controller.stop(role);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.stop(role);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(int frame)
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.seek(frame);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(PageRole role, int frame)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        const ViewportCommandResult result = controller.seek(role, frame);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.seek(role, frame);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(int milliseconds)
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.seekToPosition(milliseconds);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(PageRole role, int milliseconds)
{
    if (!ImageViewportInternal::isValidPageRole(role)) {
        const ViewportCommandResult result = controller.seekToPosition(role, milliseconds);
        applyControllerChanges(result.changes);
        return result.outcome;
    }

    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.seekToPosition(role, milliseconds);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.sync();
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    const ViewportCommandResult result = controller.resetView();
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    applyControllerChanges(result.changes);
    return result.outcome;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
    playbackScheduler.sync();
}

void ImageViewportPrivate::setNextProviderRequestTokenForTest(quint64 token)
{
    controller.setNextProviderRequestTokenForTest(token);
}

void ImageViewportPrivate::setNextProviderRequestTokenForTest(PageRole role, quint64 token)
{
    controller.setNextProviderRequestTokenForTest(role, token);
}

void ImageViewportPrivate::setNextRevisionTokenForTest(quint64 token)
{
    controller.setNextRevisionTokenForTest(token);
}

void ImageViewportPrivate::failNextProviderCommandDeliveryForTest(PageRole role)
{
    providerHost.failNextCommandDeliveryForTest(role);
}

void ImageViewportPrivate::failNextProviderQueueFlushSchedulingForTest(PageRole role)
{
    providerHost.failNextQueueFlushSchedulingForTest(role);
}

void ImageViewportPrivate::useSynchronousProviderExecutorForTest()
{
    providerHost.useSynchronousExecutorForTest();
}

void ImageViewportPrivate::useSynchronousProviderEventDeliveryForTest()
{
    providerHost.useSynchronousEventDeliveryForTest();
}

void ImageViewportPrivate::useSynchronousProviderQueueFlushSchedulerForTest()
{
    providerHost.useSynchronousQueueFlushSchedulerForTest();
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
    return internalDiagnostics.lastRenderFailure();
}

ImageViewportInternal::ProviderTransportDiagnostic
ImageViewportPrivate::lastProviderTransportDiagnosticForTest() const
{
    return internalDiagnostics.lastProviderCleanupFailure();
}

ImageViewportInternal::ProviderSchedulerDiagnostic
ImageViewportPrivate::lastProviderSchedulerDiagnosticForTest() const
{
    return internalDiagnostics.lastProviderSchedulerFailure();
}

void ImageViewportPrivate::acknowledgeRenderCommitForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    const ViewportRenderSynchronization synchronization = controller.beginRenderSynchronization();
    const auto changes = controller.acknowledgeRenderCommit(
        { { generation, requestId, preparedPayloadId } }, true, synchronization);
    applyControllerChanges(changes);
    if (changes.playbackPhase) {
        playbackScheduler.sync();
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
        playbackScheduler.sync();
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
        playbackScheduler.sync();
    }
}
#endif
