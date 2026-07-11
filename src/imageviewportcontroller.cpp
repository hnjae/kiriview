#include "framepreparation_p.h"
#include "imageviewport_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewportvalidation_p.h"
#include "viewportcontrollercommandcontract_p.h"
#include "viewportcontrollerplaybackcontract_p.h"

#include <QtQuick/QQuickWindow>

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    const ViewportPlaybackAdvanceResult result = controller.advancePlayback(elapsedMilliseconds);
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(result.schedule);
}

bool ImageViewportPrivate::hasActiveRequest() const
{
    return lastStateSnapshot.request().status() != RequestStatus::NoRequest;
}

bool ImageViewportPrivate::hasReadyDisplay() const
{
    return lastStateSnapshot.display().status() == DisplayStatus::Ready;
}

bool ImageViewportPrivate::hasDisplayableSequence() const
{
    return lastStateSnapshot.primary().metadata().available();
}

QString ImageViewportPrivate::boundedDiagnostic(const QString& diagnostic, const QString& fallback)
{
    return FramePreparation::boundedDiagnostic(diagnostic, fallback);
}

void ImageViewportPrivate::applyControllerChanges(ImageViewportInternal::ViewportChangeSet changes)
{
    changes = controller.publishChanges(changes);
    internalDiagnostics.recordRenderFailure(changes.renderFailureDiagnostic);

    if (changes.scheduleUpdate) {
        update();
    }
    refreshStateSnapshot();
}

void ImageViewportPrivate::devicePixelRatioChanged()
{
    ImageViewportInternal::ViewportChangeSet changes;
    changes.displayRevision = true;
    changes.geometryState = true;
    changes.scheduleUpdate = true;
    applyControllerChanges(changes);
    const QQuickWindow* currentWindow = window();
    const auto effects = controller.restageProviderDemands(
        currentWindow ? currentWindow->effectiveDevicePixelRatio() : 1.0);
    providerHost.applyFrameTransportEffect(effects[0]);
    providerHost.applyFrameTransportEffect(effects[1], PageRole::Secondary);
}

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.clear();
    providerHost.applyFrameTransportEffect(result.providerFrameTransport);
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(controller.playbackScheduleEffect());
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
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(result.playbackSchedule);
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
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(result.playbackSchedule);
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
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(result.playbackSchedule);
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
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(result.playbackSchedule);
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
    providerHost.applyFrameTransportEffect(
        result.secondaryProviderFrameTransport, PageRole::Secondary);
    applyControllerChanges(result.changes);
    playbackScheduler.apply(result.playbackSchedule);
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
    playbackScheduler.apply(controller.playbackScheduleEffect());
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
        playbackScheduler.apply(controller.playbackScheduleEffect());
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
        playbackScheduler.apply(controller.playbackScheduleEffect());
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
        playbackScheduler.apply(controller.playbackScheduleEffect());
    }
}
#endif
