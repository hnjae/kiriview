// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "imageviewport_p.h"
#include "imageviewportproviderfacts_p.h"
#include "imageviewporttoken_p.h"
#include "imageviewportvalidation_p.h"
#include "viewportenginetestaccess_p.h"

#include <QtCore/QMetaObject>
#include <QtQuick/QQuickWindow>

using namespace ImageViewportInternal;

void ImageViewportPrivate::advancePlayback(ViewportPlaybackTimeoutFact fact)
{
    applyEngineTransition(engine.advancePlayback(
        { fact.elapsedMilliseconds, fact.role, fact.generation, fact.scheduleIdentity }));
}

void ImageViewportPrivate::flushPlaybackSchedulers()
{
    for (auto& scheduler : playbackSchedulers)
        scheduler->flushElapsed();
}

ImageViewportStateSnapshot ImageViewportPrivate::applyEngineTransition(
    ViewportEngineTransition transition)
{
    using ScheduleAction = ViewportPlaybackScheduleEffect::Action;
    for (const auto role : { PageRole::Primary, PageRole::Secondary }) {
        const auto& schedule = transition.playbackSchedule(role);
        if (schedule.action != ScheduleAction::NoChange) {
            pendingPlaybackSchedules.forRole(role) = schedule;
        }
    }
    pendingProviderTransport.append(transition.takeProviderTransport());
    ++transitionApplicationDepth;

    providerHost.reconcileProviderLeases(
        engine.providerFrameLeaseIds(), engine.providerFailureLeaseIds());
    internalObservability.record(transition.observations());
    internalObservability.recordRenderFailure(transition.renderFailureDiagnostic());

    if (transition.schedulesRenderUpdate()) {
        prepareRenderSynchronization();
        update();
    }
    if (transition.providerSchedulerDiagnostic().valid) {
        internalObservability.recordProviderSchedulerFailure(
            transition.providerSchedulerDiagnostic());
    }

    --transitionApplicationDepth;
    return finalizeItemTransaction();
}

ImageViewportStateSnapshot ImageViewportPrivate::finalizeItemTransaction()
{
    using ScheduleAction = ViewportPlaybackScheduleEffect::Action;
    if (transitionApplicationDepth != 0 || itemTransactionDepth != 0) {
        return state();
    }
    for (const auto role : { PageRole::Primary, PageRole::Secondary }) {
        auto& pending = pendingPlaybackSchedules.forRole(role);
        if (pending.action == ScheduleAction::NoChange)
            continue;
        const ViewportPlaybackScheduleEffect schedule = pending;
        pending = { ScheduleAction::NoChange, -1, role };
        playbackSchedulers[role == PageRole::Secondary ? 1U : 0U]->apply(schedule);
    }

    const ImageViewportStateSnapshot publishedSnapshot = state();
    const bool snapshotChanged = publishedSnapshot != lastStateSnapshot;
    if (snapshotChanged) {
        lastStateSnapshot = publishedSnapshot;
        Q_EMIT q->stateChanged();
    }
    if (!drainingExternalWork) {
        drainExternalWork();
    }
    return publishedSnapshot;
}

void ImageViewportPrivate::enqueueProviderHostEvent(ViewportProviderHostEvent event)
{
    pendingProviderHostEvents.append(
        ViewportEngineProviderHostEventRequest::admit(std::move(event)));
    if (transitionApplicationDepth == 0 && !drainingExternalWork) {
        drainExternalWork();
    }
}

void ImageViewportPrivate::enqueueDeferredProviderTransport(
    ViewportProviderTransportCommand command)
{
    pendingProviderTransport.append(std::move(command));
    if (transitionApplicationDepth != 0 || itemTransactionDepth != 0 || drainingExternalWork) {
        return;
    }
    QMetaObject::invokeMethod(q, [this]() { drainExternalWork(); }, Qt::QueuedConnection);
}

void ImageViewportPrivate::drainExternalWork()
{
    if (drainingExternalWork || transitionApplicationDepth != 0 || itemTransactionDepth != 0) {
        return;
    }
    drainingExternalWork = true;
    providerHost.drainCleanup();
    while (!pendingProviderHostEvents.isEmpty() || !pendingProviderTransport.isEmpty()) {
        providerHost.drainCleanup();
        drainProviderHostEvents();
        providerHost.drainCleanup();
        if (pendingProviderTransport.isEmpty()) {
            continue;
        }
        ViewportProviderTransportCommand command = pendingProviderTransport.takeFirst();
        if (!engine.acceptsProviderTransportCommand(command)) {
            ImageViewportInternal::InternalObservation observation;
            observation.subsystem
                = ImageViewportInternal::InternalObservationSubsystem::ProviderHost;
            observation.category = ImageViewportInternal::InternalObservationCategory::StaleDrop;
            observation.cause
                = ImageViewportInternal::InternalObservationCause::RetiredProviderCommand;
            observation.identity.roleValid = true;
            observation.identity.role = command.role;
            observation.identity.generation = command.generation;
            observation.identity.sessionSerial = command.sessionSerial;
            observation.identity.providerToken
                = ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(
                    command.request.token());
            observation.detail = int(command.kind);
            internalObservability.record(observation);
            continue;
        }
        providerHost.applyTransportEffects({ command });
    }
    drainingExternalWork = false;
}

void ImageViewportPrivate::drainProviderHostEvents()
{
    if (drainingProviderHostEvents || transitionApplicationDepth != 0) {
        return;
    }
    drainingProviderHostEvents = true;
    while (!pendingProviderHostEvents.isEmpty()) {
        ViewportEngineProviderHostEventRequest request = pendingProviderHostEvents.takeFirst();
        providerHost.completeFrameEventDelivery(request.event().providerEvent.frameLeaseId);
        providerHost.completeFailureEventDelivery(
            request.event().kind == ViewportProviderHostEvent::Kind::ProviderEvent
                ? request.event().providerEvent.failureLeaseId
                : request.event().providerFailureLeaseId);
        applyEngineTransition(engine.handleProviderHostEvent(request));
        if (request.event().kind == ViewportProviderHostEvent::Kind::ProviderEvent) {
            providerHost.completeProviderEventDelivery(request.event().providerEvent.deliveryId);
        }
    }
    drainingProviderHostEvents = false;
}

void ImageViewportPrivate::viewportChanged()
{
    applyEngineTransition(engine.handleViewportChanged(viewportState()));
}

void ImageViewportPrivate::discardRetainedDisplayForResourcePressure()
{
    applyEngineTransition(engine.handleResourcePressure());
}

ImageViewportCommandResult ImageViewportPrivate::clear()
{
    ++itemTransactionDepth;
    flushPlaybackSchedulers();
    const ImageViewportCommandResult reduced
        = setPresentationTarget(ImageViewportPresentationTarget::clear(),
            PresentationTargetTransitionPolicy::defaultClear());
    --itemTransactionDepth;
    return commandResult(reduced.outcome(), finalizeItemTransaction());
}

ImageViewportCommandResult ImageViewportPrivate::play(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Play, role });
}

ImageViewportCommandResult ImageViewportPrivate::pause(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Pause, role });
}

ImageViewportCommandResult ImageViewportPrivate::stop(PageRole role)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::Stop, role });
}

ImageViewportCommandResult ImageViewportPrivate::seek(PageRole role, int frame)
{
    return executePlaybackCommand({ ViewportPlaybackCommand::Kind::SeekFrame, role, frame });
}

ImageViewportCommandResult ImageViewportPrivate::seekToPosition(PageRole role, int milliseconds)
{
    return executePlaybackCommand(
        { ViewportPlaybackCommand::Kind::SeekPosition, role, milliseconds });
}

ImageViewportCommandResult ImageViewportPrivate::executePlaybackCommand(
    ViewportPlaybackCommand command)
{
    ++itemTransactionDepth;
    if (ImageViewportInternal::isValidPageRole(command.role)) {
        flushPlaybackSchedulers();
    }
    auto reduced = engine.applyPlaybackCommand({ command });
    const CommandOutcome outcome = reduced.outcome();
    applyEngineTransition(reduced.takeTransition());
    --itemTransactionDepth;
    const ImageViewportStateSnapshot snapshot = finalizeItemTransaction();
    return commandResult(outcome, snapshot);
}

ImageViewportCommandResult ImageViewportPrivate::resetView()
{
    return setPresentation(ImageViewportPresentationCommand::resetViewCommand());
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ImageViewportPrivate::advancePlaybackForTest(int elapsedMilliseconds, PageRole role)
{
    advancePlayback({ role, 0, 0, elapsedMilliseconds });
}

void ImageViewportPrivate::setPendingPlaybackSchedulerElapsedForTest(
    int elapsedMilliseconds, PageRole role)
{
    playbackSchedulers[role == PageRole::Secondary ? 1U : 0U]->setPendingElapsedForTest(
        elapsedMilliseconds);
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

ViewportRenderAttempt ImageViewportPrivate::beginRenderSynchronizationForTest()
{
    discardPendingRenderMailbox();
    const ViewportEngineViewportState original = viewportState();
    ViewportEngineViewportState available = original;
    available.renderAvailable = true;
    engine.handleViewportChanged(available);
    const ViewportRenderAttempt attempt = engine.beginRenderSynchronization();
    ViewportEngineTestAccess::restoreViewportStatePreservingActiveRenderAttempt(engine, original);
    return attempt;
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
    const quint64 previousAttempt = currentRenderAttemptForTest();
    ViewportRenderHostFact fact;
    {
        const ViewportRenderAttempt attempt = beginRenderSynchronizationForTest();
        QVector<ViewportRenderRolePayload> rolePayloads;
        rolePayloads.reserve(attempt.snapshot.imageLayers.size());
        for (const auto& layer : attempt.snapshot.imageLayers) {
            rolePayloads.append({ layer.role, layer.preparedPayload.identity() });
        }
        const quint64 reportedAttempt
            = renderAttempt == previousAttempt ? attempt.attempt : renderAttempt;
        fact = { ViewportRenderHostFact::Outcome::Committed,
            { attempt.snapshot.targetSpread, attempt.snapshot.presentation, std::move(rolePayloads),
                PageRole::Primary, RenderFailureCause::None, reportedAttempt },
            { smoothingUnavailable, mipmapUnavailable }, !attempt.snapshot.imageLayers.isEmpty() };
    }
    discardPendingRenderMailbox(fact.acknowledgement.attempt);
    applyEngineTransition(engine.handleRenderHostFact({ std::move(fact) }));
}

void ImageViewportPrivate::discardRetainedDisplayForResourcePressureForTest()
{
    discardRetainedDisplayForResourcePressure();
}

ImageViewportInternal::RenderFailureDiagnostic
ImageViewportPrivate::lastAcceptedRenderFailureDiagnosticForTest() const
{
    return internalObservability.lastRenderFailure();
}

ImageViewportInternal::ProviderTransportDiagnostic
ImageViewportPrivate::lastProviderTransportDiagnosticForTest() const
{
    return internalObservability.lastProviderCleanupFailure();
}

ImageViewportInternal::ProviderSchedulerDiagnostic
ImageViewportPrivate::lastProviderSchedulerDiagnosticForTest() const
{
    return internalObservability.lastProviderSchedulerFailure();
}

QVector<ImageViewportInternal::InternalObservation>
ImageViewportPrivate::internalObservationsForTest() const
{
    return internalObservability.observations();
}

void ImageViewportPrivate::acknowledgeRenderCommitForTest(
    quint64 generation, quint64 requestId, quint64 preparedPayloadId)
{
    ViewportRenderHostFact fact;
    {
        const ViewportRenderAttempt attempt = beginRenderSynchronizationForTest();
        fact = { ViewportRenderHostFact::Outcome::Committed,
            { { generation, requestId }, attempt.snapshot.presentation,
                { { PageRole::Primary, { generation, preparedPayloadId } } }, PageRole::Primary,
                RenderFailureCause::None, attempt.attempt },
            {}, true };
    }
    discardPendingRenderMailbox(fact.acknowledgement.attempt);
    applyEngineTransition(engine.handleRenderHostFact({ std::move(fact) }));
}

void ImageViewportPrivate::acknowledgeRenderCommitForTest(quint64 generation, quint64 requestId,
    quint64 primaryPreparedPayloadId, quint64 secondaryPreparedPayloadId)
{
    ViewportRenderHostFact fact;
    {
        const ViewportRenderAttempt attempt = beginRenderSynchronizationForTest();
        const ImageViewportInternal::PreparedPayloadIdentity primaryPayload {
            generation,
            primaryPreparedPayloadId,
        };
        const ImageViewportInternal::PreparedPayloadIdentity secondaryPayload {
            generation,
            secondaryPreparedPayloadId,
        };
        fact = { ViewportRenderHostFact::Outcome::Committed,
            { { generation, requestId }, attempt.snapshot.presentation,
                {
                    { ImageViewportPageRole::Primary, primaryPayload },
                    { ImageViewportPageRole::Secondary, secondaryPayload },
                },
                PageRole::Primary, RenderFailureCause::None, attempt.attempt },
            {}, true };
    }
    discardPendingRenderMailbox(fact.acknowledgement.attempt);
    applyEngineTransition(engine.handleRenderHostFact({ std::move(fact) }));
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
    ViewportRenderHostFact fact;
    {
        const ViewportRenderAttempt attempt = beginRenderSynchronizationForTest();
        QVector<ViewportRenderRolePayload> rolePayloads;
        rolePayloads.reserve(attempt.snapshot.imageLayers.size());
        for (const auto& layer : attempt.snapshot.imageLayers) {
            auto identity = layer.preparedPayload.identity();
            if (layer.role == failedRole) {
                identity = { generation, preparedPayloadId };
            }
            rolePayloads.append({ layer.role, identity });
        }
        const ImageViewportInternal::PreparedPayloadIdentity failedPayload {
            generation,
            preparedPayloadId,
        };
        if (rolePayloads.isEmpty()) {
            rolePayloads.append({ failedRole, failedPayload });
        }
        fact = { ViewportRenderHostFact::Outcome::Failed,
            { { generation, requestId }, attempt.snapshot.presentation, std::move(rolePayloads),
                failedRole, cause, attempt.attempt },
            {}, true };
    }
    discardPendingRenderMailbox(fact.acknowledgement.attempt);
    applyEngineTransition(engine.handleRenderHostFact({ std::move(fact) }));
}
#endif
