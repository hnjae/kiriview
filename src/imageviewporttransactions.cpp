#include "framepreparation_p.h"
#include "imageviewport_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewportvalidation_p.h"
#include "viewportenginetestaccess_p.h"
#include "viewportitemtransaction_p.h"
#include "viewportprovidertransporteffects_p.h"

#include <QtQuick/QQuickWindow>

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(int elapsedMilliseconds)
{
    const auto reduced = engine.advancePlayback({ elapsedMilliseconds, { itemBounds(), 1.0 } });
    ViewportEngineTransition transition;
    transition.changes = reduced.changes;
    appendProviderTransport(transition.providerBeforePublication,
        reduced.effects.providerFrameTransport[0], PageRole::Primary);
    appendProviderTransport(transition.providerBeforePublication,
        reduced.effects.providerFrameTransport[1], PageRole::Secondary);
    transition.playbackSchedule = reduced.schedule;
    applyEngineTransition(std::move(transition));
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

void ImageViewportPrivate::applyEngineTransition(ViewportEngineTransition transition)
{
    using ScheduleAction = ViewportPlaybackScheduleEffect::Action;
    if (transition.playbackSchedule.action != ScheduleAction::NoChange) {
        pendingPlaybackSchedule = transition.playbackSchedule;
    }
    ++transitionApplicationDepth;

    providerHost.applyTransportEffects(transition.providerBeforePublication);
    transition.changes = engine.publishChanges(std::move(transition.changes));
    internalDiagnostics.recordRenderFailure(transition.changes.renderFailureDiagnostic);

    if (transition.changes.scheduleUpdate) {
        prepareRenderSynchronization();
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
    if (transitionApplicationDepth == 0) {
        drainProviderHostEvents();
    }
}

void ImageViewportPrivate::enqueueProviderHostEvent(ViewportProviderHostEvent event)
{
    pendingProviderHostEvents.append(std::move(event));
    if (transitionApplicationDepth == 0) {
        drainProviderHostEvents();
    }
}

void ImageViewportPrivate::drainProviderHostEvents()
{
    if (drainingProviderHostEvents || transitionApplicationDepth != 0) {
        return;
    }
    drainingProviderHostEvents = true;
    while (!pendingProviderHostEvents.isEmpty()) {
        ViewportProviderHostEvent event = pendingProviderHostEvents.takeFirst();
        applyEngineTransition(
            engine.handleProviderHostEvent({ event, { itemBounds(), 1.0 } }));
    }
    drainingProviderHostEvents = false;
}

void ImageViewportPrivate::devicePixelRatioChanged()
{
    const QQuickWindow* currentWindow = window();
    applyEngineTransition(engine.handleDevicePixelRatioChanged(
        { itemBounds(), currentWindow ? currentWindow->effectiveDevicePixelRatio() : 1.0 }));
}

ImageViewportCommandOutcome ImageViewportPrivate::clear()
{
    playbackScheduler.flushElapsed();
    return setPresentationTarget(
        ImageViewportPresentationTarget::clear(), PresentationTargetTransitionPolicy {});
}

ImageViewportCommandOutcome ImageViewportPrivate::play(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Play, role });
}

ImageViewportCommandOutcome ImageViewportPrivate::pause(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Pause, role });
}

ImageViewportCommandOutcome ImageViewportPrivate::stop(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Stop, role });
}

ImageViewportCommandOutcome ImageViewportPrivate::seek(PageRole role, int frame)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::SeekFrame, role, frame });
}

ImageViewportCommandOutcome ImageViewportPrivate::seekToPosition(PageRole role, int milliseconds)
{
    return executePlaybackCommand(
        { ViewportPlaybackCommand::Kind::SeekPosition, role, milliseconds });
}

ImageViewportCommandOutcome ImageViewportPrivate::executePlaybackCommand(
    ViewportPlaybackCommand command)
{
    if (ImageViewportInternal::isValidPageRole(command.role)) {
        playbackScheduler.flushElapsed();
    }
    const auto reduced = engine.applyPlaybackCommand({ command, { itemBounds(), 1.0 } });
    ViewportCommandResult result;
    result.outcome = reduced.command.outcome;
    result.transition.changes = reduced.changes;
    appendProviderTransport(result.transition.providerBeforePublication,
        reduced.effects.providerFrameTransport[0], PageRole::Primary);
    appendProviderTransport(result.transition.providerBeforePublication,
        reduced.effects.providerFrameTransport[1], PageRole::Secondary);
    result.transition.playbackSchedule = reduced.schedule;
    applyEngineTransition(result.transition);
    return result.outcome;
}

ImageViewportCommandOutcome ImageViewportPrivate::resetView()
{
    return setPresentation(ImageViewportPresentationCommand::resetViewCommand());
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds)
{
    advancePlayback(elapsedMilliseconds);
}

void ImageViewportPrivate::setNextProviderRequestTokenForTest(quint64 token)
{
    ViewportEngineTestAccess::providerRequests(engine, PageRole::Primary).nextRequestToken = token;
}

void ImageViewportPrivate::setNextProviderRequestTokenForTest(PageRole role, quint64 token)
{
    ViewportEngineTestAccess::providerRequests(engine, role).nextRequestToken = token;
}

void ImageViewportPrivate::setNextRevisionTokenForTest(quint64 token)
{
    ViewportEngineTestAccess::setNextRevisionValue(engine, token);
    ViewportEngineTestAccess::display(engine).revision = 0;
    ViewportEngineTestAccess::request(engine).requestRevision = 0;
    ViewportEngineTestAccess::publishedCommandRevision(engine) = 0;
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
    return ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.commitPending;
}

quint64 ImageViewportPrivate::activeRequestIdForTest() const
{
    return ViewportEngineTestAccess::request(engine).roles[0].activeRequest.identity.id;
}

quint64 ImageViewportPrivate::displayedRequestIdForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].displayedRequest.request.identity.id;
}

quint64 ImageViewportPrivate::pendingRenderGenerationForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.generation;
}

quint64 ImageViewportPrivate::pendingRenderPayloadIdForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[0].pendingRenderPayload.payloadId;
}

quint64 ImageViewportPrivate::secondaryPendingRenderPayloadIdForTest() const
{
    return ViewportEngineTestAccess::display(engine).roles[1].pendingRenderPayload.payloadId;
}

quint64 ImageViewportPrivate::currentRenderAttemptForTest() const
{
    return ViewportEngineTestAccess::currentRenderAttempt(engine);
}

void ImageViewportPrivate::reportRenderQualityFallbackForTest(
    quint64 renderAttempt, bool smoothingUnavailable, bool mipmapUnavailable)
{
    ViewportEngineTransition transition;
    transition.changes = engine
                             .handleRenderQualityFallback(
                                 { renderAttempt, smoothingUnavailable, mipmapUnavailable })
                             .changes;
    applyEngineTransition(std::move(transition));
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
    const ViewportRenderSynchronization synchronization
        = engine.beginRenderSynchronization({ { itemBounds(), 1.0 } });
    const auto reduced = engine.acknowledgeRenderCommit(
        { { { generation, requestId, preparedPayloadId } }, true, synchronization.attempt,
            synchronization.pendingTargetCommit, synchronization.pendingSecondaryProviderCommit,
            synchronization.preparedPayload, synchronization.oldDisplayStatus,
            synchronization.oldContentRect, synchronization.oldVisibleImageRect,
            synchronization.geometryState });
    ViewportEngineTransition transition;
    transition.changes = reduced.changes;
    transition.playbackSchedule = reduced.playbackSchedule;
    applyEngineTransition(std::move(transition));
}

void ImageViewportPrivate::acknowledgeRenderCommitForTest(quint64 generation, quint64 requestId,
    quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId)
{
    const ViewportRenderSynchronization synchronization
        = engine.beginRenderSynchronization({ { itemBounds(), 1.0 } });
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
    const auto reduced = engine.acknowledgeRenderCommit({ {
            primaryPayload,
            {
                { ImageViewportPageRole::Primary, primaryPayload },
                { ImageViewportPageRole::Secondary, secondaryPayload },
            },
        }, true, synchronization.attempt, synchronization.pendingTargetCommit,
        synchronization.pendingSecondaryProviderCommit, synchronization.preparedPayload,
        synchronization.oldDisplayStatus, synchronization.oldContentRect,
        synchronization.oldVisibleImageRect, synchronization.geometryState });
    ViewportEngineTransition transition;
    transition.changes = reduced.changes;
    transition.playbackSchedule = reduced.playbackSchedule;
    applyEngineTransition(std::move(transition));
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
    const auto reduced = engine.acknowledgeRenderFailure(
        { { failedPayload, { { failedRole, failedPayload } }, failedRole, cause } });
    ViewportEngineTransition transition;
    transition.changes = reduced.changes;
    transition.playbackSchedule = reduced.playbackSchedule;
    applyEngineTransition(std::move(transition));
}
#endif
