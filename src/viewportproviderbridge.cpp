#include "viewportproviderbridge_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QThread>

#include <memory>
#include <utility>

namespace {
template <typename Function>
void invokeSessionCommand(ImageSequenceProviderSession* session,
    ImageSequenceProviderThreadingContract threadingContract, Function function)
{
    if (!session) {
        return;
    }
    if (threadingContract == ImageSequenceProviderThreadingContract::ThreadSafe) {
        function();
        return;
    }
    if (session->thread() == QThread::currentThread()) {
        function();
        return;
    }
    QMetaObject::invokeMethod(session, std::move(function), Qt::BlockingQueuedConnection);
}

void queueSessionCleanup(ImageSequenceProviderSession* session,
    ImageSequenceProviderRequestToken metadataToken, ImageSequenceProviderRequestToken frameToken)
{
    if (!session) {
        return;
    }
    if (session->thread() == QThread::currentThread()) {
        session->setParent(nullptr);
    }
    const bool queued = QMetaObject::invokeMethod(
        session,
        [session, metadataToken, frameToken]() {
            if (metadataToken.isValid()) {
                session->cancelRequest(metadataToken);
            }
            if (frameToken.isValid() && frameToken != metadataToken) {
                session->cancelRequest(frameToken);
            }
            session->close();
            delete session;
        },
        Qt::QueuedConnection);
    if (!queued) {
        qWarning("ImageViewport provider cleanup could not be queued");
    }
}

ViewportProviderEvent providerEvent(
    ViewportProviderEvent::Kind kind, ImageSequenceProviderRequestToken token)
{
    ViewportProviderEvent event;
    event.kind = kind;
    event.token = token;
    return event;
}

bool deliverProviderEvent(
    ViewportProviderBridgeClient& client, quint64 sessionSerial, const ViewportProviderEvent& event)
{
    if (!client.acceptsProviderSessionResult(sessionSerial)) {
        return false;
    }
    client.handleProviderEvent(event);
    return true;
}

}

ViewportProviderBridge::ViewportProviderBridge(ViewportProviderBridgeClient& client)
    : client(client)
{
}

void ViewportProviderBridge::closeSession(ImageSequenceProviderRequestToken metadataToken,
    ImageSequenceProviderRequestToken frameToken)
{
    ImageSequenceProviderSession* session = client.takeProviderSession();
    if (!session) {
        return;
    }

    queueSessionCleanup(session, metadataToken, frameToken);
}

bool ViewportProviderBridge::openSession()
{
    const std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory
        = client.providerSessionFactory();
    if (!sessionFactory) {
        return false;
    }

    QObject* callbackTarget = client.providerCallbackTarget();
    ImageSequenceProviderSession* session = sessionFactory->createSession(callbackTarget);
    if (!session) {
        return false;
    }
    const quint64 sessionSerial = client.installProviderSession(session);
    if (sessionSerial == 0) {
        return false;
    }

    QObject::connect(
        session, &ImageSequenceProviderSession::metadataReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            const ImageSequenceProviderMetadata& metadata) {
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::MetadataReady, token);
            event.metadata = metadata;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame) {
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::ImageFrameReady, token);
            event.imageFrame = frame;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameWithMetadataReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame,
            ImageSequenceProviderFrameMetadata metadata) {
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::ImageFrameWithMetadataReady, token);
            event.imageFrame = frame;
            event.frameMetadata = metadata;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleReady, callbackTarget,
        [this, sessionSerial](
            ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::FrameHandleReady, token);
            event.frameHandle = frame;
            if (!deliverProviderEvent(client, sessionSerial, event)) {
                staleFrame.reset(frame);
            }
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleWithMetadataReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::FrameHandleWithMetadataReady, token);
            event.frameHandle = frame;
            event.frameMetadata = metadata;
            if (!deliverProviderEvent(client, sessionSerial, event)) {
                staleFrame.reset(frame);
            }
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerWaiting, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            deliverProviderEvent(
                client, sessionSerial, providerEvent(ViewportProviderEvent::Kind::Waiting, token));
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerProgress, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, double progress) {
            ViewportProviderEvent event = providerEvent(ViewportProviderEvent::Kind::Progress, token);
            event.progress = progress;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::endOfSequence, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            deliverProviderEvent(client, sessionSerial,
                providerEvent(ViewportProviderEvent::Kind::EndOfSequence, token));
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerFailed, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            ViewportProviderEvent event = providerEvent(ViewportProviderEvent::Kind::Failure, token);
            event.diagnostic = diagnostic;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupportedWithCause, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic) {
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::Unsupported, token);
            event.unsupportedCause = cause;
            event.diagnostic = diagnostic;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupported, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::Unsupported, token);
            event.diagnostic = diagnostic;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerCancelled, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            ViewportProviderEvent event
                = providerEvent(ViewportProviderEvent::Kind::Cancellation, token);
            event.diagnostic = diagnostic;
            deliverProviderEvent(client, sessionSerial, event);
        },
        Qt::QueuedConnection);

    return true;
}

void ViewportProviderBridge::requestMetadata(ImageSequenceProviderRequestToken token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = client.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token]() { session->requestMetadata(token); });
}

void ViewportProviderBridge::requestFrame(ImageSequenceProviderRequestToken token, int frame)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = client.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame]() { session->requestFrame(token, frame); });
}

void ViewportProviderBridge::requestPosition(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = client.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame, position]() { session->requestPosition(token, frame, position); });
}

void ViewportProviderBridge::requestPlayback(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = client.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame, position]() { session->requestPlayback(token, frame, position); });
}

void ViewportProviderBridge::cancelRequest(ImageSequenceProviderRequestToken token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = client.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token]() { session->cancelRequest(token); });
}

ImageSequenceProviderThreadingContract ViewportProviderBridge::threadingContract() const
{
    return client.providerThreadingContract();
}
