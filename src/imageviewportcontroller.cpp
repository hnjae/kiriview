#include "framepreparation_p.h"
#include "imageviewport_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewportvalidation_p.h"
#include "viewportcontrollercommandcontract_p.h"

#include <QtQuick/QQuickWindow>

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    applyControllerTransition(controller.advancePlayback(elapsedMilliseconds));
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

void ImageViewportPrivate::applyControllerTransition(ViewportControllerTransition transition)
{
    using ScheduleAction = ViewportPlaybackScheduleEffect::Action;
    if (transition.playbackSchedule.action != ScheduleAction::NoChange) {
        pendingPlaybackSchedule = transition.playbackSchedule;
    }
    ++transitionApplicationDepth;

    providerHost.applyTransportEffects(transition.providerBeforePublication);
    transition.changes = controller.publishChanges(transition.changes);
    internalDiagnostics.recordRenderFailure(transition.changes.renderFailureDiagnostic);

    if (transition.changes.scheduleUpdate) {
        update();
    }
    refreshStateSnapshot();
    if (transition.providerSchedulerDiagnostic.valid) {
        internalDiagnostics.recordProviderSchedulerFailure(transition.providerSchedulerDiagnostic);
    }
    providerHost.applyTransportEffects(transition.providerAfterPublication);

    --transitionApplicationDepth;
    if (transitionApplicationDepth == 0
        && pendingPlaybackSchedule.action != ScheduleAction::NoChange) {
        const ViewportPlaybackScheduleEffect schedule = pendingPlaybackSchedule;
        pendingPlaybackSchedule = {};
        playbackScheduler.apply(schedule);
    }
}

void ImageViewportPrivate::devicePixelRatioChanged()
{
    const QQuickWindow* currentWindow = window();
    applyControllerTransition(controller.handleDevicePixelRatioChanged(
        currentWindow ? currentWindow->effectiveDevicePixelRatio() : 1.0));
}

ImageViewport::CommandOutcome ImageViewportPrivate::clear()
{
    playbackScheduler.flushElapsed();
    const ViewportCommandResult result = controller.clear();
    applyControllerTransition(result.transition);
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::play(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Play, role });
}

ImageViewport::CommandOutcome ImageViewportPrivate::pause(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Pause, role });
}

ImageViewport::CommandOutcome ImageViewportPrivate::stop(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Stop, role });
}

ImageViewport::CommandOutcome ImageViewportPrivate::seek(PageRole role, int frame)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::SeekFrame, role, frame });
}

ImageViewport::CommandOutcome ImageViewportPrivate::seekToPosition(PageRole role, int milliseconds)
{
    return executePlaybackCommand(
        { ViewportPlaybackCommand::Kind::SeekPosition, role, milliseconds });
}

ImageViewport::CommandOutcome ImageViewportPrivate::executePlaybackCommand(
    ViewportPlaybackCommand command)
{
    if (ImageViewportInternal::isValidPageRole(command.role)) {
        playbackScheduler.flushElapsed();
    }
    const ViewportCommandResult result = controller.applyPlaybackCommand(command);
    applyControllerTransition(result.transition);
    return result.outcome;
}

ImageViewport::CommandOutcome ImageViewportPrivate::resetView()
{
    const ViewportCommandResult result = controller.resetView();
    applyControllerTransition(result.transition);
    return result.outcome;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
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
    auto transition = controller.acknowledgeRenderCommit(
        { { generation, requestId, preparedPayloadId } }, true, synchronization);
    applyControllerTransition(std::move(transition));
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
    auto transition = controller.acknowledgeRenderCommit(
        {
            primaryPayload,
            {
                { ImageViewport::PageRole::Primary, primaryPayload },
                { ImageViewport::PageRole::Secondary, secondaryPayload },
            },
        },
        true, synchronization);
    applyControllerTransition(std::move(transition));
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
    auto transition = controller.acknowledgeRenderFailure(
        { failedPayload, { { failedRole, failedPayload } }, failedRole, cause });
    applyControllerTransition(std::move(transition));
}
#endif
