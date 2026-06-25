#include "viewportproviderbridge_p.h"

#include "imageviewport_p.h"

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
        session->cancelRequest(metadataToken);
    }
    if (frameToken.isValid()) {
        session->cancelRequest(frameToken);
    }
    session->close();
    delete session;
}

bool ViewportProviderBridge::openSession()
{
    if (!viewport.hasProviderSequence() || !viewport.m_sequence->m_providerSessionFactory) {
        return false;
    }

    viewport.m_providerSession = viewport.m_sequence->m_providerSessionFactory->createSession(viewport.q);
    if (!viewport.m_providerSession) {
        return false;
    }

    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::metadataReady,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, const ImageSequenceProviderMetadata &metadata) {
            viewport.handleProviderMetadataReady(token, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageFrame *>(&ImageSequenceProviderSession::frameReady),
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, ImageFrame *frame) {
            viewport.handleProviderFrameReady(token, frame);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        qOverload<const ImageSequenceProviderRequestToken &, ImageFrame *, const ImageSequenceProviderFrameMetadata &>(&ImageSequenceProviderSession::frameReady),
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, ImageFrame *frame, const ImageSequenceProviderFrameMetadata &metadata) {
            viewport.handleProviderFrameReadyWithMetadata(token, frame, metadata);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerWaiting,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token) {
            viewport.handleProviderWaiting(token);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerProgress,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, double progress) {
            viewport.handleProviderProgress(token, progress);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::endOfSequence,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token) {
            viewport.handleProviderEndOfSequence(token);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerFailed,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, const QString &diagnostic) {
            viewport.handleProviderFailure(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerUnsupported,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, const QString &diagnostic) {
            viewport.handleProviderUnsupported(token, diagnostic);
        },
        Qt::QueuedConnection);
    QObject::connect(viewport.m_providerSession,
        &ImageSequenceProviderSession::providerCancelled,
        viewport.q,
        [this](const ImageSequenceProviderRequestToken &token, const QString &diagnostic) {
            viewport.handleProviderCancellation(token, diagnostic);
        },
        Qt::QueuedConnection);

    if (viewport.m_providerMetadataReady) {
        viewport.m_pendingDisplayImage = {};
        viewport.m_activeProviderFrameToken = nextRequestToken();
        viewport.m_activeProviderFrameFromPlayback = false;
        viewport.m_providerSession->requestFrame(viewport.m_activeProviderFrameToken, viewport.m_currentFrame);
    } else {
        viewport.m_activeProviderMetadataToken = nextRequestToken();
        viewport.m_providerSession->requestMetadata(viewport.m_activeProviderMetadataToken);
    }
    return true;
}

ImageSequenceProviderRequestToken ViewportProviderBridge::nextRequestToken()
{
    ++viewport.m_nextProviderRequestToken;
    return ImageSequenceProviderRequestToken(viewport.m_nextProviderRequestToken);
}
