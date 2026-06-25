#include "viewportproviderbridge_p.h"

#include "imageviewport_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QThread>

#include <limits>
#include <memory>
#include <utility>

namespace {
template <typename Function>
void invokeSessionCommand(ImageSequenceProviderSession *session, Function function)
{
    if (!session) {
        return;
    }
    if (session->thread() == QThread::currentThread()) {
        function();
        return;
    }
    QMetaObject::invokeMethod(session, std::move(function), Qt::BlockingQueuedConnection);
}

void releaseSession(ImageSequenceProviderSession *session)
{
    if (!session) {
        return;
    }
    if (session->thread() == QThread::currentThread()) {
        delete session;
        return;
    }
    QMetaObject::invokeMethod(session, [session]() {
        delete session;
    }, Qt::BlockingQueuedConnection);
}

bool acceptsSessionResult(const ImageViewportPrivate &viewport, quint64 sessionSerial)
{
    return viewport.m_providerSession && viewport.m_providerSessionSerial == sessionSerial;
}
}

ViewportProviderBridge::ViewportProviderBridge(ImageViewportPrivate &viewport)
    : viewport(viewport)
{
}

void ViewportProviderBridge::closeSession()
{
    if (!viewport.m_providerSession) {
        return;
    }

    ImageSequenceProviderSession *session = viewport.m_providerSession;
    const ImageSequenceProviderRequestToken metadataToken = viewport.m_activeProviderMetadataToken;
    const ImageSequenceProviderRequestToken frameToken = viewport.m_activeProviderFrameToken;
    viewport.m_activeProviderMetadataToken = {};
    viewport.m_activeProviderFrameToken = {};
    viewport.m_activeProviderFrameFromPlayback = false;
    viewport.m_providerSession.clear();
    if (metadataToken.isValid()) {
        invokeSessionCommand(session, [session, metadataToken]() {
            session->cancelRequest(metadataToken);
        });
    }
    if (frameToken.isValid()) {
        invokeSessionCommand(session, [session, frameToken]() {
            session->cancelRequest(frameToken);
        });
    }
    invokeSessionCommand(session, [session]() {
        session->close();
    });
    releaseSession(session);
}

bool ViewportProviderBridge::openSession()
{
    const std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory = viewport.providerSessionFactory();
    if (!sessionFactory) {
        return false;
    }

    viewport.m_providerSession = sessionFactory->createSession(viewport.q);
    if (!viewport.m_providerSession) {
        return false;
    }
    ++viewport.m_providerSessionSerial;
    const quint64 sessionSerial = viewport.m_providerSessionSerial;

    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::metadataReady,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, const ImageSequenceProviderMetadata &metadata) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderMetadataReady(token, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageFrame *>(&ImageSequenceProviderSession::frameReady),
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, ImageFrame *frame) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageFrame *, const ImageSequenceProviderFrameMetadata &>(&ImageSequenceProviderSession::frameReady),
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageSequenceProviderFrameHandle *>(&ImageSequenceProviderSession::frameReady),
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, ImageSequenceProviderFrameHandle *frame) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageSequenceProviderFrameHandle *, const ImageSequenceProviderFrameMetadata &>(&ImageSequenceProviderSession::frameReady),
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, ImageSequenceProviderFrameHandle *frame, const ImageSequenceProviderFrameMetadata &metadata) {
            std::unique_ptr<ImageSequenceProviderFrameHandle> staleFrame;
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                staleFrame.reset(frame);
                return;
            }
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerWaiting,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderWaiting(token);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerProgress,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, double progress) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderProgress(token, progress);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::endOfSequence,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderEndOfSequence(token);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerFailed,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, const QString &diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderFailure(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerUnsupported,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, const QString &diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderUnsupported(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerCancelled,
        viewport.q,
        [this, sessionSerial](const ImageSequenceProviderRequestToken &token, const QString &diagnostic) {
            if (!acceptsSessionResult(viewport, sessionSerial)) {
                return;
            }
            viewport.handleProviderCancellation(token, diagnostic);
        },
        Qt::QueuedConnection);

    if (viewport.m_providerMetadataReady) {
        viewport.discardPendingRenderCommit();
        viewport.m_activeProviderFrameToken = nextRequestToken();
        if (!viewport.m_activeProviderFrameToken.isValid()) {
            viewport.publishProviderTokenExhaustion();
            return true;
        }
        viewport.m_activeProviderFrameFromPlayback = false;
        requestFrame(viewport.m_activeProviderFrameToken, viewport.m_currentFrame);
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
        closeSession();
        return {};
    }
    ++viewport.m_nextProviderRequestToken;
    return ImageSequenceProviderRequestToken(viewport.m_nextProviderRequestToken);
}

void ViewportProviderBridge::requestMetadata(const ImageSequenceProviderRequestToken &token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession *session = viewport.m_providerSession;
    invokeSessionCommand(session, [session, token]() {
        session->requestMetadata(token);
    });
}

void ViewportProviderBridge::requestFrame(const ImageSequenceProviderRequestToken &token, int frame)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession *session = viewport.m_providerSession;
    invokeSessionCommand(session, [session, token, frame]() {
        session->requestFrame(token, frame);
    });
}

void ViewportProviderBridge::requestPlayback(const ImageSequenceProviderRequestToken &token, int frame, int position)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession *session = viewport.m_providerSession;
    invokeSessionCommand(session, [session, token, frame, position]() {
        session->requestPlayback(token, frame, position);
    });
}

void ViewportProviderBridge::cancelRequest(const ImageSequenceProviderRequestToken &token)
{
    if (!token.isValid()) {
        return;
    }
    ImageSequenceProviderSession *session = viewport.m_providerSession;
    invokeSessionCommand(session, [session, token]() {
        session->cancelRequest(token);
    });
}
