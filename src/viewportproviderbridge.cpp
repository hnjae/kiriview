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
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderMetadataReady(token, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameWithMetadataReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame,
            ImageSequenceProviderFrameMetadata metadata) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleReady, callbackTarget,
        [this, sessionSerial](
            ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            client.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleWithMetadataReady, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            client.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerWaiting, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderWaiting(token);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerProgress, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, double progress) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderProgress(token, progress);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::endOfSequence, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderEndOfSequence(token);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerFailed, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderFailure(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupportedWithCause, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderUnsupported(token, cause, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupported, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderUnsupported(token,
                ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerCancelled, callbackTarget,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!client.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            client.handleProviderCancellation(token, diagnostic);
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
