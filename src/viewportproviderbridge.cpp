#include "viewportproviderbridge_p.h"

#include "imageviewporttoken_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QPointer>
#include <QtCore/QThread>

#include <memory>
#include <utility>

namespace {
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

ViewportProviderEvent providerEvent(ImageViewport::PageRole role, quint64 sessionSerial,
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
    return ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
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

ViewportProviderEvent viewportProviderEventFromTyped(ImageViewport::PageRole role,
    quint64 sessionSerial, quint64 generation, const ImageSequenceProviderEvent& typedEvent)
{
    if (!typedEvent.isValid()) {
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
    ImageViewport::PageRole role, ImageViewportInternal::ProviderTransportOperation operation,
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
    ImageViewport::PageRole role, const ImageSequenceProviderRequest& request)
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

ViewportProviderBridge::ViewportProviderBridge(
    ViewportProviderBridgeClient& client, ImageViewport::PageRole role)
    : client(client)
    , role(role)
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
        if (client.currentProviderSession(role) == pendingSession) {
            client.takeProviderSession(role);
        }
        pendingCleanupSession.clear();
        pendingCleanupMetadataToken = {};
        pendingCleanupFrameToken = {};
    }

    ImageSequenceProviderSession* session = client.currentProviderSession(role);
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
    client.takeProviderSession(role);
    return result;
}

bool ViewportProviderBridge::openSession()
{
    const std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory
        = client.providerSessionFactory(role);
    if (!sessionFactory) {
        return false;
    }

    QObject* callbackTarget = client.providerCallbackTarget();
    ImageSequenceProviderSession* session = sessionFactory->createSession(callbackTarget);
    if (!session) {
        return false;
    }
    const quint64 sessionSerial = client.installProviderSession(role, session);
    if (sessionSerial == 0) {
        return false;
    }
    const quint64 generation = client.currentProviderGeneration(role);
    const Qt::ConnectionType eventDeliveryConnectionType = this->eventDeliveryConnectionType();

    QObject::connect(
        session, &ImageSequenceProviderSession::providerEvent, callbackTarget,
        [this, sessionSerial, generation](const ImageSequenceProviderEvent& typedEvent) {
            client.handleProviderEvent(
                viewportProviderEventFromTyped(role, sessionSerial, generation, typedEvent));
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::metadataReady, callbackTarget,
        [this, sessionSerial, generation](ImageSequenceProviderRequestToken token,
            const ImageSequenceProviderMetadata& metadata) {
            ViewportProviderEvent event = providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::MetadataReady, token);
            event.metadata = metadata;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameReady, callbackTarget,
        [this, sessionSerial, generation](
            ImageSequenceProviderRequestToken token, ImageFrame* frame) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::ImageFrameReady, token);
            event.imageFrame = frame;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameWithMetadataReady, callbackTarget,
        [this, sessionSerial, generation](ImageSequenceProviderRequestToken token,
            ImageFrame* frame, ImageSequenceProviderFrameMetadata metadata) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::ImageFrameWithMetadataReady, token);
            event.imageFrame = frame;
            event.frameMetadata = metadata;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleReady, callbackTarget,
        [this, sessionSerial, generation](
            ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::FrameHandleReady, token);
            event.frameHandle = frame;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleWithMetadataReady, callbackTarget,
        [this, sessionSerial, generation](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata) {
            ViewportProviderEvent event = providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::FrameHandleWithMetadataReady, token);
            event.frameHandle = frame;
            event.frameMetadata = metadata;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerWaiting, callbackTarget,
        [this, sessionSerial, generation](ImageSequenceProviderRequestToken token) {
            client.handleProviderEvent(providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::Waiting, token));
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerProgress, callbackTarget,
        [this, sessionSerial, generation](
            ImageSequenceProviderRequestToken token, double progress) {
            ViewportProviderEvent event = providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::Progress, token);
            event.progress = progress;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::endOfSequence, callbackTarget,
        [this, sessionSerial, generation](ImageSequenceProviderRequestToken token) {
            client.handleProviderEvent(providerEvent(role, sessionSerial, generation,
                ViewportProviderEvent::Kind::EndOfSequence, token));
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerFailed, callbackTarget,
        [this, sessionSerial, generation](
            ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            ViewportProviderEvent event = providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::Failure, token);
            event.diagnostic = diagnostic;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupportedWithCause, callbackTarget,
        [this, sessionSerial, generation](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic) {
            ViewportProviderEvent event = providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::Unsupported, token);
            event.unsupportedCause = cause;
            event.unsupportedCauseExplicit = true;
            event.diagnostic = diagnostic;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupported, callbackTarget,
        [this, sessionSerial, generation](
            ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            ViewportProviderEvent event = providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::Unsupported, token);
            event.diagnostic = diagnostic;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerCancelled, callbackTarget,
        [this, sessionSerial, generation](
            ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            ViewportProviderEvent event = providerEvent(
                role, sessionSerial, generation, ViewportProviderEvent::Kind::Cancellation, token);
            event.diagnostic = diagnostic;
            client.handleProviderEvent(event);
        },
        eventDeliveryConnectionType);

    return true;
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
    ImageSequenceProviderSession* session = client.currentProviderSession(role);
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
    return client.providerThreadingContract(role);
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
