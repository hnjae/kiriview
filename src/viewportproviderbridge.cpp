#include "viewportproviderbridge_p.h"

#include "imageviewporttoken_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <limits>
#include <memory>
#include <utility>

namespace {
QMutex& providerSessionOwnershipMutex()
{
    static QMutex mutex;
    return mutex;
}

QSet<const ImageSequenceProviderSession*>& ownedProviderSessions()
{
    static QSet<const ImageSequenceProviderSession*> sessions;
    return sessions;
}

quint64 allocateProviderFrameLeaseId()
{
    static QMutex mutex;
    static quint64 nextId = 0;
    QMutexLocker locker(&mutex);
    if (nextId == std::numeric_limits<quint64>::max()) {
        qFatal("ImageViewport provider frame lease identity exhausted");
    }
    return ++nextId;
}

bool claimProviderSession(ImageSequenceProviderSession* session)
{
    QMutexLocker locker(&providerSessionOwnershipMutex());
    if (ownedProviderSessions().contains(session)) {
        return false;
    }
    ownedProviderSessions().insert(session);
    return true;
}

void releaseProviderSession(const ImageSequenceProviderSession* session)
{
    QMutexLocker locker(&providerSessionOwnershipMutex());
    ownedProviderSessions().remove(session);
}

QVector<ImageSequenceProviderRequestToken> cleanupTokens(
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    QVector<ImageSequenceProviderRequestToken> tokens;
    if (metadataToken.isValid()) {
        tokens.append(metadataToken);
    }
    if (frameToken.isValid() && frameToken != metadataToken) {
        tokens.append(frameToken);
    }
    return tokens;
}

void requestProviderCleanup(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    QVector<ImageSequenceProviderRequestToken> tokens = cleanupTokens(metadataToken, frameToken);
    if (!tokens.isEmpty()) {
        session->request(ImageSequenceProviderRequest::cancel(std::move(tokens)));
    }
    session->request(ImageSequenceProviderRequest::close());
}

class QtViewportProviderExecutor final : public ViewportProviderExecutor
{
public:
    ViewportProviderExecutorOutcome invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        std::function<void()> command) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            command();
            return ViewportProviderExecutorOutcome::Completed;
        }
        if (session->thread() == QThread::currentThread()) {
            command();
            return ViewportProviderExecutorOutcome::Completed;
        }
        return QMetaObject::invokeMethod(session, std::move(command), Qt::BlockingQueuedConnection)
            ? ViewportProviderExecutorOutcome::Completed
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome queueSessionClose(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [session, metadataToken, frameToken]() {
                       requestProviderCleanup(session, metadataToken, frameToken);
                   },
                   Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        const bool queued = QMetaObject::invokeMethod(
            session,
            [session, metadataToken, frameToken]() {
                requestProviderCleanup(session, metadataToken, frameToken);
                delete session;
            },
            Qt::QueuedConnection);
        if (!queued) {
            qWarning("ImageViewport provider cleanup could not be queued");
        }
        return queued ? ViewportProviderExecutorOutcome::Scheduled
                      : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome queueSessionDestruction(
        ImageSequenceProviderSession* session) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        return QMetaObject::invokeMethod(
                   session, [session]() { delete session; }, Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        if (!frameHandle) {
            return ViewportProviderExecutorOutcome::Completed;
        }
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            frameHandle->release();
            if (!session || session->thread() == QThread::currentThread()) {
                delete frameHandle;
                return ViewportProviderExecutorOutcome::Completed;
            }
            return QMetaObject::invokeMethod(
                       session, [frameHandle]() { delete frameHandle; }, Qt::QueuedConnection)
                ? ViewportProviderExecutorOutcome::Scheduled
                : ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (!session) {
            frameHandle->release();
            delete frameHandle;
            return ViewportProviderExecutorOutcome::Completed;
        }
        if (session->thread() == QThread::currentThread()) {
            frameHandle->release();
            delete frameHandle;
            return ViewportProviderExecutorOutcome::Completed;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [frameHandle]() {
                       frameHandle->release();
                       delete frameHandle;
                   },
                   Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }
};

ViewportProviderExecutor& qtViewportProviderExecutor()
{
    static QtViewportProviderExecutor executor;
    return executor;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
class SynchronousViewportProviderExecutor final : public ViewportProviderExecutor
{
public:
    ViewportProviderExecutorOutcome invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract, std::function<void()> command) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        command();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionClose(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        requestProviderCleanup(session, metadataToken, frameToken);
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        session->setParent(nullptr);
        requestProviderCleanup(session, metadataToken, frameToken);
        delete session;
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome queueSessionDestruction(
        ImageSequenceProviderSession* session) override
    {
        delete session;
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(ImageSequenceProviderSession*,
        ImageSequenceProviderThreadingContract,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        delete frameHandle;
        return ViewportProviderExecutorOutcome::Completed;
    }
};
#endif

ViewportProviderExecutor* defaultProviderExecutor() { return &qtViewportProviderExecutor(); }

bool executorAccepted(ViewportProviderExecutorOutcome outcome)
{
    return outcome != ViewportProviderExecutorOutcome::RetryableFailure;
}

ViewportProviderEvent providerEvent(ImageViewportPageRole role, quint64 sessionSerial,
    quint64 generation, ViewportProviderEvent::Kind kind, ImageSequenceProviderRequestToken token)
{
    ViewportProviderEvent event;
    event.kind = kind;
    event.role = role;
    event.sessionSerial = sessionSerial;
    event.generation = generation;
    event.token = token;
    return event;
}

ViewportProviderEvent viewportProviderEventFromTyped(ImageViewportPageRole role,
    quint64 sessionSerial, quint64 generation, const ImageSequenceProviderEvent& typedEvent)
{
    if (!typedEvent.isValid()) {
        if (typedEvent.kind() == ImageSequenceProviderEventKind::MetadataReady) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::MetadataReady, typedEvent.token());
            event.metadata = typedEvent.metadata();
            return event;
        }
        if (typedEvent.kind() == ImageSequenceProviderEventKind::Progress) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::Progress, typedEvent.token());
            event.progress = typedEvent.progress();
            return event;
        }
        if (typedEvent.kind() == ImageSequenceProviderEventKind::FrameReady) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::FrameHandleWithMetadataReady, typedEvent.token());
            event.frameHandle = typedEvent.frameHandle();
            event.frameEnvelope = typedEvent.frameEnvelope();
            return event;
        }
        if (typedEvent.kind() == ImageSequenceProviderEventKind::Unsupported
            && typedEvent.token().isValid()) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::Unsupported, typedEvent.token());
            event.unsupportedCause = typedEvent.unsupportedCause();
            event.unsupportedCauseExplicit = true;
            event.diagnostic = typedEvent.diagnostic();
            return event;
        }
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::Failure, typedEvent.token());
        event.diagnostic = QStringLiteral("provider event is invalid");
        return event;
    }

    switch (typedEvent.kind()) {
    case ImageSequenceProviderEventKind::MetadataReady: {
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::MetadataReady, typedEvent.token());
        event.metadata = typedEvent.metadata();
        return event;
    }
    case ImageSequenceProviderEventKind::FrameReady: {
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::FrameHandleWithMetadataReady, typedEvent.token());
        event.frameHandle = typedEvent.frameHandle();
        event.frameEnvelope = typedEvent.frameEnvelope();
        return event;
    }
    case ImageSequenceProviderEventKind::Waiting:
        return providerEvent(role, sessionSerial, generation, ViewportProviderEvent::Kind::Waiting,
            typedEvent.token());
    case ImageSequenceProviderEventKind::Progress: {
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::Progress, typedEvent.token());
        event.progress = typedEvent.progress();
        return event;
    }
    case ImageSequenceProviderEventKind::EndOfSequence:
        return providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::EndOfSequence, typedEvent.token());
    case ImageSequenceProviderEventKind::Unsupported: {
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::Unsupported, typedEvent.token());
        event.unsupportedCause = typedEvent.unsupportedCause();
        event.unsupportedCauseExplicit = true;
        event.diagnostic = typedEvent.diagnostic();
        return event;
    }
    case ImageSequenceProviderEventKind::Cancelled: {
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::Cancellation, typedEvent.token());
        event.diagnostic = typedEvent.diagnostic();
        return event;
    }
    case ImageSequenceProviderEventKind::Failed: {
        ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
            ViewportProviderEvent::Kind::Failure, typedEvent.token());
        event.diagnostic = typedEvent.diagnostic();
        return event;
    }
    }
    return providerEvent(
        role, sessionSerial, generation, ViewportProviderEvent::Kind::Failure, typedEvent.token());
}

ImageViewportInternal::ProviderTransportDiagnostic providerTransportDiagnostic(
    ImageViewportPageRole role, ImageViewportInternal::ProviderTransportOperation operation,
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken,
    bool queued, bool pendingCleanup, quint64 generation = 0, quint64 sessionSerial = 0,
    quint64 providerLeaseId = 0)
{
    return {
        true,
        role,
        operation,
        metadataToken.isValid(),
        metadataToken.isValid()
            ? ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(metadataToken)
            : 0,
        frameToken.isValid(),
        frameToken.isValid()
            ? ImageViewportInternal::ProviderRequestTokenPrivateAccess::value(frameToken)
            : 0,
        queued,
        pendingCleanup,
        generation,
        sessionSerial,
        providerLeaseId,
    };
}

ImageViewportInternal::ProviderTransportDiagnostic providerTransportDiagnostic(
    ImageViewportPageRole role, const ImageSequenceProviderRequest& request, quint64 generation,
    quint64 sessionSerial)
{
    if (request.kind() != ImageSequenceProviderRequestKind::Cancel) {
        return {};
    }
    const QVector<ImageSequenceProviderRequestToken> tokens = request.tokens();
    return providerTransportDiagnostic(role,
        ImageViewportInternal::ProviderTransportOperation::Cancel, {},
        tokens.isEmpty() ? ImageSequenceProviderRequestToken {} : tokens.first(), false, false,
        generation, sessionSerial);
}
}

ViewportProviderBridge::ViewportProviderBridge(ImageViewportPageRole role)
    : role(role)
    , providerExecutor(defaultProviderExecutor())
{
}

ViewportProviderTransportResult ViewportProviderBridge::closeSession(
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    ViewportProviderTransportResult result;
    ImageSequenceProviderSession* session = activeSession;
    if (!session) {
        return result;
    }
    auto recordIt = sessions.find(session);
    if (recordIt == sessions.end()) {
        activeSession.clear();
        return result;
    }
    SessionRecord& record = recordIt.value();
    record.metadataToken = metadataToken;
    record.frameToken = frameToken;
    activeSession.clear();

    if (takeForcedDeliveryFailureForTest()) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true, record.generation, record.sessionSerial);
        record.lifecycle = SessionLifecycle::CleanupPending;
        return result;
    }

    if (hasFrameLeases(session)) {
        result.delivered
            = executorAccepted(executor().queueSessionClose(session, metadataToken, frameToken));
        if (!result.delivered) {
            result.diagnostic = providerTransportDiagnostic(role,
                ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
                false, true, record.generation, record.sessionSerial);
            record.lifecycle = SessionLifecycle::CleanupPending;
            return result;
        }
        record.lifecycle = SessionLifecycle::Closing;
        return result;
    }

    QPointer<ImageSequenceProviderSession> sessionGuard(session);
    result.delivered
        = executorAccepted(executor().queueSessionCleanup(session, metadataToken, frameToken));
    if (!result.delivered) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true, record.generation, record.sessionSerial);
        record.lifecycle = SessionLifecycle::CleanupPending;
        return result;
    }
    if (sessionGuard && sessionGuard->thread() == QThread::currentThread()) {
        sessionGuard->setParent(nullptr);
    }
    sessions.erase(recordIt);
    return result;
}

ViewportProviderSessionOpenTransportResult ViewportProviderBridge::openSession(
    const ViewportProviderSessionOpenInput& input)
{
    if (!input.factory || !input.callbackTarget || !input.eventSink || input.generation == 0
        || input.sessionSerial == 0) {
        return { false, QStringLiteral("provider session open input is invalid") };
    }

    const ImageSequenceProviderSessionFactoryResult factoryResult = (*input.factory)();
    ImageSequenceProviderSession* session = factoryResult.session();
    const bool validCreated
        = factoryResult.outcome() == ImageSequenceProviderSessionFactoryOutcome::Created && session
        && factoryResult.diagnostic().isEmpty();
    const bool validFailed
        = factoryResult.outcome() == ImageSequenceProviderSessionFactoryOutcome::Failed && !session;
    if (!validCreated || validFailed) {
        return { false,
            factoryResult.outcome() == ImageSequenceProviderSessionFactoryOutcome::Failed
                ? factoryResult.diagnostic()
                : QStringLiteral("provider session factory returned a malformed result") };
    }
    if (!claimProviderSession(session)) {
        return { false, QStringLiteral("provider session is already owned by another generation") };
    }
    QObject::connect(
        session, &QObject::destroyed, [session]() { releaseProviderSession(session); });
    if (!session->parent() && session->thread() == input.callbackTarget->thread()) {
        session->setParent(input.callbackTarget);
    }
    activeSession = session;
    sessions.insert(session,
        { session, input.threadingContract, input.generation, input.sessionSerial,
            SessionLifecycle::Active, {}, {} });
    const Qt::ConnectionType eventDeliveryConnectionType = this->eventDeliveryConnectionType();

    QObject::connect(
        session, &ImageSequenceProviderSession::providerEvent, input.callbackTarget,
        [this, sessionGuard = QPointer<ImageSequenceProviderSession>(session),
            threadingContract = input.threadingContract, sessionSerial = input.sessionSerial,
            generation = input.generation,
            eventSink = input.eventSink](const ImageSequenceProviderEvent& typedEvent) {
            ViewportProviderEvent event
                = viewportProviderEventFromTyped(role, sessionSerial, generation, typedEvent);
            if (event.frameHandle) {
                event.frameLeaseId = claimFrameHandle(sessionGuard.data(), threadingContract,
                    event.frameHandle, generation, sessionSerial);
            }
            eventSink(event);
        },
        eventDeliveryConnectionType);

    return { true, {} };
}

ViewportProviderTransportResult ViewportProviderBridge::deliverRequest(
    const ImageSequenceProviderRequest& request)
{
    ViewportProviderTransportResult result;
    if (!request.isValid()) {
        return result;
    }
    ImageSequenceProviderSession* session = activeSession;
    auto recordIt = sessions.constFind(session);
    const quint64 generation = recordIt == sessions.cend() ? 0 : recordIt->generation;
    const quint64 sessionSerial = recordIt == sessions.cend() ? 0 : recordIt->sessionSerial;
    if (takeForcedDeliveryFailureForTest()) {
        result.diagnostic = providerTransportDiagnostic(role, request, generation, sessionSerial);
        return result;
    }
    const auto threadingContract = recordIt == sessions.cend()
        ? ImageSequenceProviderThreadingContract::AffinityBound
        : recordIt->threadingContract;
    result.delivered = executorAccepted(executor().invokeSessionCommand(
        session, threadingContract, [session, request]() { session->request(request); }));
    if (!result.delivered) {
        result.diagnostic = providerTransportDiagnostic(role, request, generation, sessionSerial);
    }
    return result;
}

void ViewportProviderBridge::setExecutor(ViewportProviderExecutor& executor)
{
    providerExecutor = &executor;
}

Qt::ConnectionType ViewportProviderBridge::eventDeliveryConnectionType() const
{
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    if (synchronousEventDelivery) {
        return Qt::DirectConnection;
    }
#endif
    return Qt::QueuedConnection;
}

ViewportProviderExecutor& ViewportProviderBridge::executor() const
{
    return providerExecutor ? *providerExecutor : qtViewportProviderExecutor();
}

quint64 ViewportProviderBridge::claimFrameHandle(ImageSequenceProviderSession* session,
    ImageSequenceProviderThreadingContract threadingContract,
    ImageSequenceProviderFrameHandle* frameHandle, quint64 generation, quint64 sessionSerial)
{
    if (!frameHandle) {
        return 0;
    }
    const quint64 leaseId = allocateProviderFrameLeaseId();
    frameLeases.insert(
        leaseId, { session, frameHandle, threadingContract, generation, sessionSerial, true });
    return leaseId;
}

bool ViewportProviderBridge::hasFrameLeases(ImageSequenceProviderSession* session) const
{
    for (const auto& lease : frameLeases) {
        if (lease.session == session) {
            return true;
        }
    }
    return false;
}

void ViewportProviderBridge::completeFrameEventDelivery(quint64 leaseId)
{
    auto it = frameLeases.find(leaseId);
    if (it != frameLeases.end()) {
        it->pendingEngineDelivery = false;
    }
}

void ViewportProviderBridge::reconcileFrameLeases(const QSet<quint64>& liveLeaseIds)
{
    for (auto it = frameLeases.cbegin(); it != frameLeases.cend(); ++it) {
        if (!it->pendingEngineDelivery && !liveLeaseIds.contains(it.key())) {
            retiredFrameLeases.insert(it.key());
        }
    }
}

ViewportProviderCleanupResult ViewportProviderBridge::drainCleanup(bool retryPendingSessions)
{
    ViewportProviderCleanupResult result;
    const auto retired = retiredFrameLeases.values();
    for (quint64 leaseId : retired) {
        ViewportProviderCleanupResult released = releaseFrameLease(leaseId);
        result.diagnostics.append(released.diagnostics);
        result.progress = result.progress || released.progress;
    }
    retrySessionCleanup(result, retryPendingSessions);
    result.pending = hasPendingCleanup();
    return result;
}

bool ViewportProviderBridge::hasPendingCleanup() const
{
    if (!retiredFrameLeases.isEmpty()) {
        return true;
    }
    for (const SessionRecord& record : sessions) {
        if (record.lifecycle != SessionLifecycle::Active) {
            return true;
        }
    }
    return false;
}

ViewportProviderCleanupResult ViewportProviderBridge::releaseAllFrameLeases()
{
    const auto leaseIds = frameLeases.keys();
    for (quint64 leaseId : leaseIds) {
        retiredFrameLeases.insert(leaseId);
    }
    return drainCleanup(true);
}

ViewportProviderCleanupResult ViewportProviderBridge::releaseFrameLease(quint64 leaseId)
{
    ViewportProviderCleanupResult result;
    auto it = frameLeases.find(leaseId);
    if (it == frameLeases.end()) {
        retiredFrameLeases.remove(leaseId);
        return result;
    }
    const FrameLeaseRecord lease = it.value();
    const auto outcome
        = executor().releaseFrameHandle(lease.session, lease.threadingContract, lease.frameHandle);
    if (!executorAccepted(outcome)) {
        result.diagnostics.append(providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Release, {}, {}, false, true,
            lease.generation, lease.sessionSerial, leaseId));
        result.pending = true;
        return result;
    }
    frameLeases.erase(it);
    retiredFrameLeases.remove(leaseId);
    result.progress = true;
    result.pending = hasPendingCleanup();
    return result;
}

void ViewportProviderBridge::destroyClosingSessionIfUnused(
    ImageSequenceProviderSession* session, ViewportProviderCleanupResult& result)
{
    if (!session || hasFrameLeases(session)) {
        return;
    }
    auto recordIt = sessions.find(session);
    if (recordIt == sessions.end()
        || (recordIt->lifecycle != SessionLifecycle::Closing
            && recordIt->lifecycle != SessionLifecycle::DestructionPending)) {
        return;
    }
    const SessionRecord record = recordIt.value();
    const auto outcome = executor().queueSessionDestruction(session);
    if (!executorAccepted(outcome)) {
        recordIt->lifecycle = SessionLifecycle::DestructionPending;
        result.diagnostics.append(providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Destruction, {}, {}, false, true,
            record.generation, record.sessionSerial));
        result.pending = true;
        return;
    }
    sessions.erase(recordIt);
    result.progress = true;
}

void ViewportProviderBridge::retrySessionCleanup(
    ViewportProviderCleanupResult& result, bool retryPendingSessions)
{
    const auto sessionKeys = sessions.keys();
    for (ImageSequenceProviderSession* session : sessionKeys) {
        auto recordIt = sessions.find(session);
        if (recordIt == sessions.end()) {
            continue;
        }
        if (!recordIt->session) {
            if (activeSession == session) {
                activeSession.clear();
            }
            sessions.erase(recordIt);
            result.progress = true;
            continue;
        }
        if (recordIt->lifecycle == SessionLifecycle::CleanupPending && retryPendingSessions) {
            const SessionRecord record = recordIt.value();
            const bool leasesRemain = hasFrameLeases(session);
            QPointer<ImageSequenceProviderSession> sessionGuard(session);
            const auto outcome = leasesRemain
                ? executor().queueSessionClose(session, record.metadataToken, record.frameToken)
                : executor().queueSessionCleanup(session, record.metadataToken, record.frameToken);
            if (!executorAccepted(outcome)) {
                result.diagnostics.append(providerTransportDiagnostic(role,
                    ImageViewportInternal::ProviderTransportOperation::Close, record.metadataToken,
                    record.frameToken, false, true, record.generation, record.sessionSerial));
                result.pending = true;
                continue;
            }
            result.progress = true;
            if (leasesRemain) {
                recordIt->lifecycle = SessionLifecycle::Closing;
            } else {
                if (sessionGuard && sessionGuard->thread() == QThread::currentThread()) {
                    sessionGuard->setParent(nullptr);
                }
                sessions.erase(recordIt);
                continue;
            }
        }
        recordIt = sessions.find(session);
        if (recordIt != sessions.end()
            && (recordIt->lifecycle == SessionLifecycle::Closing
                || (retryPendingSessions
                    && recordIt->lifecycle == SessionLifecycle::DestructionPending))) {
            destroyClosingSessionIfUnused(session, result);
        }
    }
}

bool ViewportProviderBridge::takeForcedDeliveryFailureForTest()
{
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    if (forceNextCommandDeliveryFailure) {
        forceNextCommandDeliveryFailure = false;
        return true;
    }
#endif
    return false;
}

#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
void ViewportProviderBridge::failNextCommandDeliveryForTest()
{
    forceNextCommandDeliveryFailure = true;
}

void ViewportProviderBridge::useSynchronousEventDeliveryForTest()
{
    synchronousEventDelivery = true;
}

ViewportProviderExecutor& synchronousViewportProviderExecutorForTest()
{
    static SynchronousViewportProviderExecutor executor;
    return executor;
}
#endif
