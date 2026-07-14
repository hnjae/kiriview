#include "viewportproviderbridge_p.h"

#include "imageviewporttoken_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QThread>

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
    bool invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract threadingContract,
        std::function<void()> command) override
    {
        if (!session) {
            return false;
        }
        if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
            command();
            return true;
        }
        if (session->thread() == QThread::currentThread()) {
            command();
            return true;
        }
        return QMetaObject::invokeMethod(session, std::move(command), Qt::BlockingQueuedConnection);
    }

    bool queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        if (!session) {
            return false;
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
        return queued;
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
    bool invokeSessionCommand(ImageSequenceProviderSession* session,
        ImageSequenceProviderThreadingContract, std::function<void()> command) override
    {
        if (!session) {
            return false;
        }
        command();
        return true;
    }

    bool queueSessionCleanup(ImageSequenceProviderSession* session,
        ImageSequenceProviderRequestToken metadataToken,
        ImageSequenceProviderRequestToken frameToken) override
    {
        if (!session) {
            return false;
        }
        session->setParent(nullptr);
        requestProviderCleanup(session, metadataToken, frameToken);
        delete session;
        return true;
    }
};
#endif

ViewportProviderExecutor* defaultProviderExecutor() { return &qtViewportProviderExecutor(); }

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

ImageSequenceProviderSession::UnsupportedCause legacyUnsupportedCause(
    ImageSequenceProviderUnsupportedCause cause)
{
    switch (cause) {
    case ImageSequenceProviderUnsupportedCause::UnsupportedRequest:
        return ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest;
    case ImageSequenceProviderUnsupportedCause::PayloadRejection:
        return ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
    }
    return static_cast<ImageSequenceProviderSession::UnsupportedCause>(static_cast<int>(cause));
}

ImageSequenceProviderFrameMetadata frameMetadataFor(
    const ImageSequenceProviderFrameEnvelope& envelope)
{
    if (envelope.frameStartPosition() < 0) {
        return ImageSequenceProviderFrameMetadata::stillFrame();
    }
    return ImageSequenceProviderFrameMetadata::timedFrame(
        envelope.frame(), envelope.frameStartPosition(), envelope.frameDuration());
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
            event.frameMetadata = frameMetadataFor(typedEvent.frameEnvelope());
            event.frameEnvelope = typedEvent.frameEnvelope();
            return event;
        }
        if (typedEvent.kind() == ImageSequenceProviderEventKind::Unsupported
            && typedEvent.token().isValid()) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::Unsupported, typedEvent.token());
            event.unsupportedCause = legacyUnsupportedCause(typedEvent.unsupportedCause());
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
        event.frameMetadata = frameMetadataFor(typedEvent.frameEnvelope());
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
        event.unsupportedCause = legacyUnsupportedCause(typedEvent.unsupportedCause());
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
    bool queued, bool pendingCleanup)
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
    };
}

ImageViewportInternal::ProviderTransportDiagnostic providerTransportDiagnostic(
    ImageViewportPageRole role, const ImageSequenceProviderRequest& request)
{
    if (request.kind() != ImageSequenceProviderRequestKind::Cancel) {
        return {};
    }
    const QVector<ImageSequenceProviderRequestToken> tokens = request.tokens();
    return providerTransportDiagnostic(role,
        ImageViewportInternal::ProviderTransportOperation::Cancel, {},
        tokens.isEmpty() ? ImageSequenceProviderRequestToken {} : tokens.first(), false, false);
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
    const auto queueCleanup
        = [this](ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken metadata,
              ImageSequenceProviderRequestToken frame) {
              ViewportProviderTransportResult cleanupResult;
              QPointer<ImageSequenceProviderSession> sessionGuard(session);
              cleanupResult.delivered = executor().queueSessionCleanup(session, metadata, frame);
              if (!cleanupResult.delivered) {
                  cleanupResult.diagnostic = providerTransportDiagnostic(role,
                      ImageViewportInternal::ProviderTransportOperation::Close, metadata, frame,
                      false, true);
                  return cleanupResult;
              }
              if (sessionGuard && sessionGuard->thread() == QThread::currentThread()) {
                  sessionGuard->setParent(nullptr);
              }
              return cleanupResult;
          };
    const auto rememberPendingCleanup
        = [this](ImageSequenceProviderSession* session, ImageSequenceProviderRequestToken metadata,
              ImageSequenceProviderRequestToken frame) {
              pendingCleanupSession = session;
              pendingCleanupMetadataToken = metadata;
              pendingCleanupFrameToken = frame;
          };

    if (pendingCleanupSession) {
        ImageSequenceProviderSession* pendingSession = pendingCleanupSession;
        result
            = queueCleanup(pendingSession, pendingCleanupMetadataToken, pendingCleanupFrameToken);
        if (!result.delivered) {
            return result;
        }
        if (activeSession == pendingSession) {
            activeSession.clear();
        }
        pendingCleanupSession.clear();
        pendingCleanupMetadataToken = {};
        pendingCleanupFrameToken = {};
    }

    ImageSequenceProviderSession* session = activeSession;
    if (!session) {
        return result;
    }

    if (takeForcedDeliveryFailureForTest()) {
        result.diagnostic = providerTransportDiagnostic(role,
            ImageViewportInternal::ProviderTransportOperation::Close, metadataToken, frameToken,
            false, true);
        rememberPendingCleanup(session, metadataToken, frameToken);
        return result;
    }

    result = queueCleanup(session, metadataToken, frameToken);
    if (!result.delivered) {
        rememberPendingCleanup(session, metadataToken, frameToken);
        return result;
    }
    activeSession.clear();
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
    activeThreadingContract = input.threadingContract;
    const Qt::ConnectionType eventDeliveryConnectionType = this->eventDeliveryConnectionType();

    QObject::connect(
        session, &ImageSequenceProviderSession::providerEvent, input.callbackTarget,
        [this, sessionSerial = input.sessionSerial, generation = input.generation,
            eventSink = input.eventSink](const ImageSequenceProviderEvent& typedEvent) {
            eventSink(viewportProviderEventFromTyped(role, sessionSerial, generation, typedEvent));
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
    if (takeForcedDeliveryFailureForTest()) {
        result.diagnostic = providerTransportDiagnostic(role, request);
        return result;
    }
    ImageSequenceProviderSession* session = activeSession;
    result.delivered = executor().invokeSessionCommand(
        session, threadingContract(), [session, request]() { session->request(request); });
    if (!result.delivered) {
        result.diagnostic = providerTransportDiagnostic(role, request);
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

ImageSequenceProviderThreadingContract ViewportProviderBridge::threadingContract() const
{
    return activeThreadingContract;
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
