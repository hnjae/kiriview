#include "viewportproviderbridge_p.h"

#include "imageviewport_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QThread>

#include <limits>
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

bool acceptsSessionResult(const ImageViewportPrivate& viewport, quint64 sessionSerial)
{
    return viewport.m_providerSession && viewport.m_providerSessionSerial == sessionSerial;
}

}

ViewportProviderBridge::ViewportProviderBridge(ImageViewportPrivate& viewport)
    : viewport(viewport)
{
}

void ViewportProviderBridge::closeSession(ImageSequenceProviderRequestToken metadataToken,
    ImageSequenceProviderRequestToken frameToken)
{
    if (!viewport.m_providerSession) {
        return;
    }

    ImageSequenceProviderSession* session = viewport.m_providerSession;
    viewport.m_providerSession.clear();
    queueSessionCleanup(session, metadataToken, frameToken);
}

bool ViewportProviderBridge::openSession()
{
    const std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory
        = viewport.providerSessionFactory();
    if (!sessionFactory) {
        return false;
    }

    viewport.m_providerSession = sessionFactory->createSession(viewport.q);
    if (!viewport.m_providerSession) {
        return false;
    }
    ++viewport.m_providerSessionSerial;
    const quint64 sessionSerial = viewport.m_providerSessionSerial;

    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::metadataReady, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            const ImageSequenceProviderMetadata& metadata) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderMetadataReady(token, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::imageFrameReady, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::imageFrameWithMetadataReady,
        viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, ImageFrame* frame,
            ImageSequenceProviderFrameMetadata metadata) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::frameHandleReady, viewport.q,
        [this, sessionSerial](
            ImageSequenceProviderRequestToken token, ImageSequenceProviderFrameHandle* frame) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::frameHandleWithMetadataReady,
        viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderFrameHandle* frame, ImageSequenceProviderFrameMetadata metadata) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::providerWaiting, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderWaiting(token);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::providerProgress, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, double progress) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderProgress(token, progress);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::endOfSequence, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderEndOfSequence(token);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::providerFailed, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderFailure(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::providerUnsupportedWithCause,
        viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token,
            ImageSequenceProviderSession::UnsupportedCause cause, const QString& diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderUnsupported(token, cause, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::providerUnsupported, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            const auto cause = token == viewport.m_activeProviderMetadataToken
                ? ImageSequenceProviderSession::UnsupportedCause::UnsupportedRequest
                : ImageSequenceProviderSession::UnsupportedCause::PayloadRejection;
            viewport.handleProviderUnsupported(token, cause, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(
        viewport.m_providerSession, &ImageSequenceProviderSession::providerCancelled, viewport.q,
        [this, sessionSerial](ImageSequenceProviderRequestToken token, const QString& diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderCancellation(token, diagnostic);
        },
        Qt::QueuedConnection);

    if (viewport.m_providerMetadataReady) {
        viewport.discardPendingRenderCommit();
        viewport.startProviderFrameRequest(viewport.request.activeRequest.target.frame,
            viewport.request.activeRequest.target.providerTargetKind);
    } else {
        viewport.m_activeProviderMetadataToken = nextRequestToken();
        if (!viewport.m_activeProviderMetadataToken.isValid()) {
            viewport.publishProviderTokenExhaustion();
            return true;
        }
        requestMetadata(viewport.m_activeProviderMetadataToken);
    }
    return true;
}

ImageSequenceProviderRequestToken ViewportProviderBridge::nextRequestToken()
{
    if (viewport.m_nextProviderRequestToken == std::numeric_limits<quint64>::max()) {
        viewport.closeProviderSession();
        return {};
    }
    ++viewport.m_nextProviderRequestToken;
    return ImageSequenceProviderRequestToken(viewport.m_nextProviderRequestToken);
}

void ViewportProviderBridge::requestMetadata(ImageSequenceProviderRequestToken token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.m_providerSession;
    invokeSessionCommand(session, threadingContract(),
        [session, token]() { session->requestMetadata(token); });
}

void ViewportProviderBridge::requestFrame(ImageSequenceProviderRequestToken token, int frame)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.m_providerSession;
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame]() { session->requestFrame(token, frame); });
}

void ViewportProviderBridge::requestPosition(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.m_providerSession;
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame, position]() { session->requestPosition(token, frame, position); });
}

void ViewportProviderBridge::requestPlayback(
    ImageSequenceProviderRequestToken token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.m_providerSession;
    invokeSessionCommand(session, threadingContract(),
        [session, token, frame, position]() { session->requestPlayback(token, frame, position); });
}

void ViewportProviderBridge::cancelRequest(ImageSequenceProviderRequestToken token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession* session = viewport.m_providerSession;
    invokeSessionCommand(session, threadingContract(),
        [session, token]() { session->cancelRequest(token); });
}

ImageSequenceProviderThreadingContract ViewportProviderBridge::threadingContract() const
{
    return viewport.providerThreadingContract();
}
