#include "viewportproviderbridge_p.h"

#include "imageviewport_p.h"

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

ViewportProviderBridge::ViewportProviderBridge(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
}

void ViewportProviderBridge::closeSession(ImageSequenceProviderRequestToken metadataToken,
    ImageSequenceProviderRequestToken frameToken)
{
    ImageSequenceProviderSession* session = viewport.takeProviderSession();
    if (!session) {
        return;
    }

    queueSessionCleanup(session, metadataToken, frameToken);
}

bool ViewportProviderBridge::openSession()
{
    const std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory
        = viewport.providerSessionFactory();
    if (!sessionFactory) {
        return false;
    }

    ImageSequenceProviderSession* session = sessionFactory->createSession(viewport.q);
    if (!session) {
        return false;
    }
    const quint64 sessionSerial = viewport.installProviderSession(session);
    if (sessionSerial == 0) {
        return false;
    }

    QObject::connect(
        session, &ImageSequenceProviderSession::metadataReady, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            const ImageSequenceProviderMetadata& metadata) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderMetadataReady(token, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameReady, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::imageFrameWithMetadataReady, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame,
            ImageSequenceProviderFrameMetadata metadata) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleReady, viewport.q,
        [this, sessionSerial](
            ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::frameHandleWithMetadataReady, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerWaiting, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderWaiting(token);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerProgress, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, double progress) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderProgress(token, progress);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::endOfSequence, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderEndOfSequence(token);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerFailed, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderFailure(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupportedWithCause, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderUnsupported(token, cause, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerUnsupported, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderUnsupported(token,
                ImageSequenceProviderSession::UnsupportedCause::PayloadRejection, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        session, &ImageSequenceProviderSession::providerCancelled, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!viewport.acceptsProviderSessionResult(sessionSerial)) {
                return;
            }
            viewport.handleProviderCancellation(token, diagnostic);
        },
        Qt::QueuedConnection);

    return true;
}

void ViewportProviderBridge::requestMetadata(ImageSequenceProviderRequestToken token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token]() { session->requestMetadata(token); });
}

void ViewportProviderBridge::requestFrame(ImageSequenceProviderRequestToken token, int frame)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame]() { session->requestFrame(token, frame); });
}

void ViewportProviderBridge::requestPosition(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame, position]() { session->requestPosition(token, frame, position); });
}

void ViewportProviderBridge::requestPlayback(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame, position]() { session->requestPlayback(token, frame, position); });
}

void ViewportProviderBridge::cancelRequest(ImageSequenceProviderRequestToken token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.currentProviderSession();
    invokeSessionCommand(session, threadingContract(),
        [session, token]() { session->cancelRequest(token); });
}

ImageSequenceProviderThreadingContract ViewportProviderBridge::threadingContract() const
{
    return viewport.providerThreadingContract();
}
