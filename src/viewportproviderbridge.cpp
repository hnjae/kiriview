#include "viewportproviderbridge_p.h"

#include "imageviewporttoken_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <limits>
#include <memory>
#include <optional>
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

    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [sessionControl, session, metadataToken, frameToken]() {
                       requestProviderCleanup(session, metadataToken, frameToken);
                       sessionControl->completeCloseOnSessionAffinity();
                   },
                   Qt::QueuedConnection)
            ? ViewportProviderExecutorOutcome::Scheduled
            : ViewportProviderExecutorOutcome::RetryableFailure;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        const auto threadingContract = sessionControl
            ? sessionControl->threadingContract()
            : ImageSequenceProviderThreadingContract::AffinityBound;
        if (!frameHandle) {
            if (!sessionControl || !session) {
                return ViewportProviderExecutorOutcome::RetryableFailure;
            }
            if (session->thread() == QThread::currentThread()) {
                sessionControl->completeFrameReleaseOnSessionAffinity();
                return ViewportProviderExecutorOutcome::Completed;
            }
            return QMetaObject::invokeMethod(
                       session,
                       [sessionControl]() {
                           sessionControl->completeFrameReleaseOnSessionAffinity();
                       },
                       Qt::QueuedConnection)
                ? ViewportProviderExecutorOutcome::Scheduled
                : ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            frameHandle->release();
            if (!session || session->thread() == QThread::currentThread()) {
                delete frameHandle;
                if (sessionControl) {
                    sessionControl->completeFrameReleaseOnSessionAffinity();
                }
                return ViewportProviderExecutorOutcome::Completed;
            }
            return QMetaObject::invokeMethod(
                       session,
                       [sessionControl, frameHandle]() {
                           delete frameHandle;
                           sessionControl->completeFrameReleaseOnSessionAffinity();
                       },
                       Qt::QueuedConnection)
                ? ViewportProviderExecutorOutcome::Scheduled
                : ViewportProviderExecutorOutcome::RetryableFailure;
        }
        if (!session)
            return ViewportProviderExecutorOutcome::RetryableFailure;
        if (session->thread() == QThread::currentThread()) {
            frameHandle->release();
            delete frameHandle;
            sessionControl->completeFrameReleaseOnSessionAffinity();
            return ViewportProviderExecutorOutcome::Completed;
        }
        return QMetaObject::invokeMethod(
                   session,
                   [sessionControl, frameHandle]() {
                       frameHandle->release();
                       delete frameHandle;
                       sessionControl->completeFrameReleaseOnSessionAffinity();
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

    ViewportProviderExecutorOutcome queueSessionClose(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        ImageSequenceProviderSession* session
            = sessionControl ? sessionControl->session() : nullptr;
        if (!session) {
            return ViewportProviderExecutorOutcome::RetryableFailure;
        }
        requestProviderCleanup(session, metadataToken, frameToken);
        sessionControl->completeCloseOnSessionAffinity();
        return ViewportProviderExecutorOutcome::Completed;
    }

    ViewportProviderExecutorOutcome releaseFrameHandle(
        const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle) override
    {
        delete frameHandle;
        if (sessionControl) {
            sessionControl->completeFrameReleaseOnSessionAffinity();
        }
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

class ViewportProviderEventEndpoint
{
public:
    explicit ViewportProviderEventEndpoint(std::function<void(const ViewportProviderEvent&)> sink)
        : eventSink(std::move(sink))
    {
    }

    bool deliver(const ViewportProviderEvent& event)
    {
        std::function<void(const ViewportProviderEvent&)> sink;
        {
            QMutexLocker locker(&mutex);
            sink = eventSink;
        }
        if (!sink) {
            return false;
        }
        sink(event);
        return true;
    }

    void revoke()
    {
        QMutexLocker locker(&mutex);
        eventSink = {};
    }

private:
    QMutex mutex;
    std::function<void(const ViewportProviderEvent&)> eventSink;
};

class ViewportProviderLeaseRegistry
    : public std::enable_shared_from_this<ViewportProviderLeaseRegistry>
{
public:
    struct LeaseSnapshot
    {
        std::shared_ptr<ViewportProviderSessionControl> sessionControl;
        QPointer<ImageSequenceProviderFrameHandle> frameHandle;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
    };

    explicit ViewportProviderLeaseRegistry(ViewportProviderExecutor* executor)
        : providerExecutor(executor)
        , cleanupDispatchTarget(QCoreApplication::instance())
    {
    }

    void setExecutor(ViewportProviderExecutor& executor)
    {
        QMutexLocker locker(&mutex);
        providerExecutor = &executor;
    }

    quint64 claim(const std::shared_ptr<ViewportProviderSessionControl>& sessionControl,
        ImageSequenceProviderFrameHandle* frameHandle)
    {
        if (!sessionControl || !frameHandle) {
            return 0;
        }
        sessionControl->claimFrameLease();
        const quint64 leaseId = allocateProviderFrameLeaseId();
        bool releaseAutomatically = false;
        {
            QMutexLocker locker(&mutex);
            frameLeases.insert(leaseId,
                { sessionControl, frameHandle, sessionControl->generation(),
                    sessionControl->sessionSerial(), true, false });
            if (automaticCleanup) {
                retiredFrameLeases.insert(leaseId);
                releaseAutomatically = true;
            }
        }
        if (releaseAutomatically) {
            scheduleAutomaticRelease(leaseId);
        }
        return leaseId;
    }

    void completeEventDelivery(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        frameLeases.detach();
        auto it = frameLeases.find(leaseId); // clazy:exclude=detaching-member
        if (it != frameLeases.end()) {
            it->pendingEngineDelivery = false;
        }
    }

    void reconcile(const QSet<quint64>& liveLeaseIds)
    {
        QMutexLocker locker(&mutex);
        for (auto it = frameLeases.cbegin(); it != frameLeases.cend(); ++it) {
            if (!it->pendingEngineDelivery && !liveLeaseIds.contains(it.key())) {
                retiredFrameLeases.insert(it.key());
            }
        }
    }

    void retire(quint64 leaseId)
    {
        if (leaseId == 0) {
            return;
        }
        bool releaseAutomatically = false;
        {
            QMutexLocker locker(&mutex);
            if (frameLeases.contains(leaseId)) {
                retiredFrameLeases.insert(leaseId);
                releaseAutomatically = automaticCleanup;
            }
        }
        if (releaseAutomatically) {
            scheduleAutomaticRelease(leaseId);
        }
    }

    void retireAll()
    {
        QVector<quint64> leaseIds;
        {
            QMutexLocker locker(&mutex);
            automaticCleanup = true;
            leaseIds.reserve(frameLeases.size());
            for (auto it = frameLeases.cbegin(); it != frameLeases.cend(); ++it) {
                retiredFrameLeases.insert(it.key());
                leaseIds.append(it.key());
            }
        }
        for (quint64 leaseId : leaseIds) {
            scheduleAutomaticRelease(leaseId);
        }
    }

    QVector<quint64> retiredLeaseIds() const
    {
        QMutexLocker locker(&mutex);
        QVector<quint64> leaseIds;
        leaseIds.reserve(retiredFrameLeases.size());
        for (quint64 leaseId : retiredFrameLeases) {
            const auto it = frameLeases.constFind(leaseId);
            if (it != frameLeases.cend() && !it->releaseScheduling) {
                leaseIds.append(leaseId);
            }
        }
        return leaseIds;
    }

    std::optional<LeaseSnapshot> takeLeaseForRelease(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        frameLeases.detach();
        auto it = frameLeases.find(leaseId); // clazy:exclude=detaching-member
        if (it == frameLeases.end() || it->releaseScheduling) {
            return std::nullopt;
        }
        it->releaseScheduling = true;
        return LeaseSnapshot { it->sessionControl, it->frameHandle, it->generation,
            it->sessionSerial };
    }

    void releaseSchedulingFailed(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        frameLeases.detach();
        auto it = frameLeases.find(leaseId); // clazy:exclude=detaching-member
        if (it != frameLeases.end()) {
            it->releaseScheduling = false;
        }
    }

    void erase(quint64 leaseId)
    {
        QMutexLocker locker(&mutex);
        frameLeases.remove(leaseId);
        retiredFrameLeases.remove(leaseId);
    }

    bool hasPendingCleanup() const
    {
        QMutexLocker locker(&mutex);
        return !retiredFrameLeases.isEmpty();
    }

private:
    struct LeaseRecord
    {
        std::shared_ptr<ViewportProviderSessionControl> sessionControl;
        QPointer<ImageSequenceProviderFrameHandle> frameHandle;
        quint64 generation = 0;
        quint64 sessionSerial = 0;
        bool pendingEngineDelivery = true;
        bool releaseScheduling = false;
    };

    void scheduleAutomaticRelease(quint64 leaseId)
    {
        const auto self = weak_from_this().lock();
        if (!self) {
            return;
        }
        if (cleanupDispatchTarget) {
            QMetaObject::invokeMethod(
                cleanupDispatchTarget, [self, leaseId]() { self->releaseAutomatically(leaseId); },
                Qt::QueuedConnection);
        }
    }

    void releaseAutomatically(quint64 leaseId)
    {
        const auto lease = takeLeaseForRelease(leaseId);
        if (!lease) {
            return;
        }
        ViewportProviderExecutor* executor = nullptr;
        {
            QMutexLocker locker(&mutex);
            executor = providerExecutor;
        }
        const auto outcome = executor
            ? executor->releaseFrameHandle(lease->sessionControl, lease->frameHandle)
            : ViewportProviderExecutorOutcome::RetryableFailure;
        if (executorAccepted(outcome)) {
            erase(leaseId);
            return;
        }
        releaseSchedulingFailed(leaseId);
        const auto self = weak_from_this().lock();
        if (!self) {
            return;
        }
        if (cleanupDispatchTarget) {
            QTimer::singleShot(10, cleanupDispatchTarget,
                [self, leaseId]() { self->releaseAutomatically(leaseId); });
        }
    }

    mutable QMutex mutex;
    QHash<quint64, LeaseRecord> frameLeases;
    QSet<quint64> retiredFrameLeases;
    ViewportProviderExecutor* providerExecutor = nullptr;
    QPointer<QObject> cleanupDispatchTarget;
    bool automaticCleanup = false;
};

ViewportProviderSessionControl::ViewportProviderSessionControl(
    ImageSequenceProviderSession* session, ImageSequenceProviderThreadingContract threadingContract,
    quint64 generation, quint64 sessionSerial)
    : providerSession(session)
    , contract(threadingContract)
    , generationIdentity(generation)
    , sessionIdentity(sessionSerial)
{
}

ImageSequenceProviderSession* ViewportProviderSessionControl::session() const
{
    QMutexLocker locker(&mutex);
    return providerSession;
}

ImageSequenceProviderThreadingContract ViewportProviderSessionControl::threadingContract() const
{
    return contract;
}

quint64 ViewportProviderSessionControl::generation() const { return generationIdentity; }

quint64 ViewportProviderSessionControl::sessionSerial() const { return sessionIdentity; }

bool ViewportProviderSessionControl::beginEventIngress()
{
    QMutexLocker locker(&mutex);
    if (!providerSession || destructionStarted) {
        return false;
    }
    ++activeIngressCount;
    return true;
}

void ViewportProviderSessionControl::claimFrameLease()
{
    QMutexLocker locker(&mutex);
    ++frameLeaseCount;
}

void ViewportProviderSessionControl::endEventIngress()
{
    bool scheduleCheck = false;
    {
        QMutexLocker locker(&mutex);
        Q_ASSERT(activeIngressCount > 0);
        --activeIngressCount;
        scheduleCheck = closeCompleted && activeIngressCount == 0 && frameLeaseCount == 0;
    }
    if (scheduleCheck) {
        scheduleDestructionCheck();
    }
}

void ViewportProviderSessionControl::completeCloseOnSessionAffinity()
{
    {
        QMutexLocker locker(&mutex);
        closeCompleted = true;
    }
    destroySessionIfReadyOnSessionAffinity();
}

void ViewportProviderSessionControl::completeFrameReleaseOnSessionAffinity()
{
    {
        QMutexLocker locker(&mutex);
        Q_ASSERT(frameLeaseCount > 0);
        --frameLeaseCount;
    }
    destroySessionIfReadyOnSessionAffinity();
}

void ViewportProviderSessionControl::markSessionDestroyed()
{
    QMutexLocker locker(&mutex);
    providerSession = nullptr;
    destructionStarted = true;
}

void ViewportProviderSessionControl::destroySessionIfReadyOnSessionAffinity()
{
    ImageSequenceProviderSession* sessionToDestroy = nullptr;
    {
        QMutexLocker locker(&mutex);
        if (!providerSession || destructionStarted || !closeCompleted || activeIngressCount != 0
            || frameLeaseCount != 0) {
            return;
        }
        destructionStarted = true;
        sessionToDestroy = providerSession;
        providerSession = nullptr;
    }
    Q_ASSERT(sessionToDestroy->thread() == QThread::currentThread());
    sessionToDestroy->setParent(nullptr);
    delete sessionToDestroy;
}

void ViewportProviderSessionControl::scheduleDestructionCheck()
{
    ImageSequenceProviderSession* currentSession = session();
    if (!currentSession) {
        return;
    }
    if (currentSession->thread() == QThread::currentThread()) {
        destroySessionIfReadyOnSessionAffinity();
        return;
    }
    const auto self = shared_from_this();
    QMetaObject::invokeMethod(
        currentSession, [self]() { self->destroySessionIfReadyOnSessionAffinity(); },
        Qt::QueuedConnection);
}

ViewportProviderBridge::ViewportProviderBridge(ImageViewportPageRole role)
    : role(role)
    , providerExecutor(defaultProviderExecutor())
    , frameLeaseRegistry(std::make_shared<ViewportProviderLeaseRegistry>(providerExecutor))
{
}

ViewportProviderBridge::~ViewportProviderBridge()
{
    frameLeaseRegistry->setExecutor(qtViewportProviderExecutor());
    for (const auto& endpoint : std::as_const(eventEndpoints)) {
        if (const auto value = endpoint.lock()) {
            value->revoke();
        }
    }
    frameLeaseRegistry->retireAll();
}

ViewportProviderTransportResult ViewportProviderBridge::closeSession(
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    ViewportProviderTransportResult result;
    ImageSequenceProviderSession* session = activeSession;
    if (!session) {
        return result;
    }
    sessions.detach();
    auto recordIt = sessions.find(session); // clazy:exclude=detaching-member
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

    result.delivered
        = executorAccepted(executor().queueSessionClose(record.control, metadataToken, frameToken));
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
    if (session->parent() && session->thread() == QThread::currentThread()) {
        session->setParent(nullptr);
    }
    const auto sessionControl = std::make_shared<ViewportProviderSessionControl>(
        session, input.threadingContract, input.generation, input.sessionSerial);
    const auto eventEndpoint = std::make_shared<ViewportProviderEventEndpoint>(input.eventSink);
    QObject::connect(session, &QObject::destroyed, session, [session, sessionControl]() {
        sessionControl->markSessionDestroyed();
        releaseProviderSession(session);
    });
    activeSession = session;
    sessions.insert(session,
        { session, input.threadingContract, input.generation, input.sessionSerial,
            SessionLifecycle::Active, {}, {}, sessionControl, eventEndpoint });
    eventEndpoints.append(eventEndpoint);
#ifdef IMAGEVIEWPORT_PRIVATE_TEST_PROBES
    const bool deliverSynchronously = synchronousEventDelivery;
#else
    constexpr bool deliverSynchronously = false;
#endif

    QObject::connect(
        session, &ImageSequenceProviderSession::providerEvent, input.callbackTarget,
        [sessionControl, eventEndpoint, leaseRegistry = frameLeaseRegistry,
            callbackTarget = input.callbackTarget, eventRole = role,
            sessionSerial = input.sessionSerial, generation = input.generation,
            deliverSynchronously](const ImageSequenceProviderEvent& typedEvent) {
            if (!sessionControl->beginEventIngress()) {
                return;
            }
            ViewportProviderEvent event
                = viewportProviderEventFromTyped(eventRole, sessionSerial, generation, typedEvent);
            if (event.frameHandle) {
                event.frameLeaseId = leaseRegistry->claim(sessionControl, event.frameHandle);
            }
            auto deliver = [eventEndpoint, leaseRegistry, event]() {
                if (!eventEndpoint->deliver(event) && event.frameLeaseId != 0) {
                    leaseRegistry->retire(event.frameLeaseId);
                }
            };
            if (deliverSynchronously) {
                deliver();
            } else if (!QMetaObject::invokeMethod(
                           callbackTarget, std::move(deliver), Qt::QueuedConnection)
                && event.frameLeaseId != 0) {
                leaseRegistry->retire(event.frameLeaseId);
            }
            sessionControl->endEventIngress();
        },
        Qt::DirectConnection);

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
    frameLeaseRegistry->setExecutor(executor);
}

ViewportProviderExecutor& ViewportProviderBridge::executor() const
{
    return providerExecutor ? *providerExecutor : qtViewportProviderExecutor();
}

void ViewportProviderBridge::completeFrameEventDelivery(quint64 leaseId)
{
    frameLeaseRegistry->completeEventDelivery(leaseId);
}

void ViewportProviderBridge::reconcileFrameLeases(const QSet<quint64>& liveLeaseIds)
{
    frameLeaseRegistry->reconcile(liveLeaseIds);
}

ViewportProviderCleanupResult ViewportProviderBridge::drainCleanup(bool retryPendingSessions)
{
    ViewportProviderCleanupResult result;
    const auto retired = frameLeaseRegistry->retiredLeaseIds();
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
    if (frameLeaseRegistry->hasPendingCleanup()) {
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
    frameLeaseRegistry->retireAll();
    return drainCleanup(true);
}

ViewportProviderCleanupResult ViewportProviderBridge::releaseFrameLease(quint64 leaseId)
{
    ViewportProviderCleanupResult result;
    const auto lease = frameLeaseRegistry->takeLeaseForRelease(leaseId);
    if (!lease) {
        return result;
    }
    const auto outcome = executor().releaseFrameHandle(lease->sessionControl, lease->frameHandle);
    if (!executorAccepted(outcome)) {
        frameLeaseRegistry->releaseSchedulingFailed(leaseId);
        result.diagnostics.append(providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Release, {}, {}, false, true,
            lease->generation, lease->sessionSerial, leaseId));
        result.pending = true;
        return result;
    }
    frameLeaseRegistry->erase(leaseId);
    result.progress = true;
    result.pending = hasPendingCleanup();
    return result;
}

void ViewportProviderBridge::retrySessionCleanup(
    ViewportProviderCleanupResult& result, bool retryPendingSessions)
{
    const auto sessionKeys = sessions.keys();
    for (ImageSequenceProviderSession* session : sessionKeys) {
        sessions.detach();
        auto recordIt = sessions.find(session); // clazy:exclude=detaching-member
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
            const SessionRecord& record = recordIt.value();
            const auto outcome = executor().queueSessionClose(
                record.control, record.metadataToken, record.frameToken);
            if (!executorAccepted(outcome)) {
                result.diagnostics.append(providerTransportDiagnostic(role,
                    ImageViewportInternal::ProviderTransportOperation::Close, record.metadataToken,
                    record.frameToken, false, true, record.generation, record.sessionSerial));
                result.pending = true;
                continue;
            }
            result.progress = true;
            recordIt->lifecycle = SessionLifecycle::Closing;
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
