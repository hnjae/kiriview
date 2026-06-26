#pragma once

#include "imageviewport.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QList>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QPointF>
#include <QtCore/QScopeGuard>
#include <QtCore/QThread>
#include <QtGui/QImage>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGImageNode>
#include <QtQuick/QSGSimpleRectNode>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

static_assert(std::is_abstract_v<ImageSequenceProviderAdapter>,
    "ImageSequenceProviderAdapter must remain an abstract public extension-point base");

namespace {

QString componentErrors(const QQmlComponent& component)
{
    QStringList messages;
    const QList<QQmlError> errors = component.errors();
    for (const QQmlError& error : errors) {
        messages.append(error.toString());
    }
    return messages.join(QLatin1Char('\n'));
}

int enumValue(const QMetaObject* metaObject, const char* enumName, const char* key)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    if (index < 0) {
        return -1;
    }
    return metaObject->enumerator(index).keyToValue(key);
}

void verifyEnumValues(
    const QMetaObject* metaObject, const char* enumName, const QList<QByteArray>& keys)
{
    const int index = metaObject->indexOfEnumerator(enumName);
    QVERIFY2(index >= 0, enumName);
    const QMetaEnum enumerator = metaObject->enumerator(index);
    for (const QByteArray& key : keys) {
        QVERIFY2(enumerator.keyToValue(key.constData()) >= 0, key.constData());
    }
}

void verifyRequestStatusReasonPair(const ImageViewport& item)
{
    const QMetaObject* metaObject = item.metaObject();
    const int status = item.property("requestStatus").toInt();
    const int reason = item.property("requestReason").toInt();

    const bool valid = (status == enumValue(metaObject, "RequestStatus", "NoRequest")
                           && reason == enumValue(metaObject, "RequestReason", "NoRequest"))
        || (status == enumValue(metaObject, "RequestStatus", "Loading")
            && (reason == enumValue(metaObject, "RequestReason", "ProviderWaiting")
                || reason == enumValue(metaObject, "RequestReason", "RequestQueued")
                || reason == enumValue(metaObject, "RequestReason", "UploadPending")
                || reason == enumValue(metaObject, "RequestReason", "RenderWaiting")))
        || (status == enumValue(metaObject, "RequestStatus", "Ready")
            && reason == enumValue(metaObject, "RequestReason", "Ready"))
        || (status == enumValue(metaObject, "RequestStatus", "Unsupported")
            && (reason == enumValue(metaObject, "RequestReason", "UnsupportedRequest")
                || reason == enumValue(metaObject, "RequestReason", "InvalidRequest")
                || reason == enumValue(metaObject, "RequestReason", "PayloadRejection")))
        || (status == enumValue(metaObject, "RequestStatus", "Error")
            && (reason == enumValue(metaObject, "RequestReason", "ProviderFailure")
                || reason == enumValue(metaObject, "RequestReason", "PayloadRejection")
                || reason == enumValue(metaObject, "RequestReason", "RenderFailure")));
    const QString message
        = QStringLiteral("invalid request status/reason pair: %1/%2").arg(status).arg(reason);
    QVERIFY2(valid, qPrintable(message));
}

void verifyInvalidCoordinateResult(const QVariantMap& result)
{
    QCOMPARE(result.value("valid").toBool(), false);
    QCOMPARE(result.value("x").toDouble(), 0.0);
    QCOMPARE(result.value("y").toDouble(), 0.0);
}

class PaintProbeViewport final : public ImageViewport
{
public:
    using ImageViewport::ImageViewport;

    QSGNode* takePaintNode(QSGNode* oldNode = nullptr) { return updatePaintNode(oldNode, nullptr); }
};

bool commitPaintNode(PaintProbeViewport& item)
{
    QScopedPointer<QSGNode> root(item.takePaintNode());
    return !root.isNull();
}

class CountingProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit CountingProviderSession(const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount,
        const std::shared_ptr<int>& lastRequestedFrame, const std::shared_ptr<int>& closeCount,
        const std::shared_ptr<int>& playbackRequestCount = {},
        const std::shared_ptr<int>& lastPlaybackFrame = {},
        const std::shared_ptr<int>& lastPlaybackPosition = {},
        const std::shared_ptr<int>& cancelRequestCount = {},
        const std::shared_ptr<quint64>& lastCancelledTokenId = {}, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_lastRequestedFrame(lastRequestedFrame)
        , m_closeCount(closeCount)
        , m_playbackRequestCount(playbackRequestCount)
        , m_lastPlaybackFrame(lastPlaybackFrame)
        , m_lastPlaybackPosition(lastPlaybackPosition)
        , m_cancelRequestCount(cancelRequestCount)
        , m_lastCancelledTokenId(lastCancelledTokenId)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken& token) override
    {
        m_lastMetadataToken = token;
        ++*m_metadataRequestCount;
    }

    void requestFrame(const ImageSequenceProviderRequestToken& token, int frame) override
    {
        m_lastFrameToken = token;
        *m_lastRequestedFrame = frame;
        ++*m_frameRequestCount;
    }

    void requestPlayback(
        const ImageSequenceProviderRequestToken& token, int frame, int position) override
    {
        if (m_playbackRequestCount) {
            ++*m_playbackRequestCount;
        }
        if (m_lastPlaybackFrame) {
            *m_lastPlaybackFrame = frame;
        }
        if (m_lastPlaybackPosition) {
            *m_lastPlaybackPosition = position;
        }
        ImageSequenceProviderSession::requestPlayback(token, frame, position);
    }

    void cancelRequest(const ImageSequenceProviderRequestToken& token) override
    {
        m_lastCancelledToken = token;
        if (m_cancelRequestCount) {
            ++*m_cancelRequestCount;
        }
        if (m_lastCancelledTokenId) {
            *m_lastCancelledTokenId = token.id();
        }
    }

    void close() override { ++*m_closeCount; }

    ImageSequenceProviderRequestToken lastMetadataToken() const { return m_lastMetadataToken; }

    ImageSequenceProviderRequestToken lastFrameToken() const { return m_lastFrameToken; }

    ImageSequenceProviderRequestToken lastCancelledToken() const { return m_lastCancelledToken; }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    std::shared_ptr<int> m_playbackRequestCount;
    std::shared_ptr<int> m_lastPlaybackFrame;
    std::shared_ptr<int> m_lastPlaybackPosition;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<quint64> m_lastCancelledTokenId;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
    ImageSequenceProviderRequestToken m_lastFrameToken;
    ImageSequenceProviderRequestToken m_lastCancelledToken;
};

class CountingProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit CountingProviderSessionFactory(const std::shared_ptr<int>& sessionCount,
        const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount,
        const std::shared_ptr<int>& lastRequestedFrame, const std::shared_ptr<int>& closeCount,
        const std::shared_ptr<int>& playbackRequestCount = {},
        const std::shared_ptr<int>& lastPlaybackFrame = {},
        const std::shared_ptr<int>& lastPlaybackPosition = {},
        const std::shared_ptr<int>& cancelRequestCount = {},
        const std::shared_ptr<quint64>& lastCancelledTokenId = {})
        : m_sessionCount(sessionCount)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_lastRequestedFrame(lastRequestedFrame)
        , m_closeCount(closeCount)
        , m_playbackRequestCount(playbackRequestCount)
        , m_lastPlaybackFrame(lastPlaybackFrame)
        , m_lastPlaybackPosition(lastPlaybackPosition)
        , m_cancelRequestCount(cancelRequestCount)
        , m_lastCancelledTokenId(lastCancelledTokenId)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        ++*m_sessionCount;
        CountingProviderSession* session
            = new CountingProviderSession(m_metadataRequestCount, m_frameRequestCount,
                m_lastRequestedFrame, m_closeCount, m_playbackRequestCount, m_lastPlaybackFrame,
                m_lastPlaybackPosition, m_cancelRequestCount, m_lastCancelledTokenId, parent);
        m_lastSession = session;
        m_sessions.append(session);
        return session;
    }

    CountingProviderSession* lastSession() const { return m_lastSession; }

    CountingProviderSession* sessionAt(qsizetype index) const { return m_sessions.at(index); }

private:
    std::shared_ptr<int> m_sessionCount;
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    std::shared_ptr<int> m_playbackRequestCount;
    std::shared_ptr<int> m_lastPlaybackFrame;
    std::shared_ptr<int> m_lastPlaybackPosition;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<quint64> m_lastCancelledTokenId;
    QPointer<CountingProviderSession> m_lastSession;
    QList<QPointer<CountingProviderSession>> m_sessions;
};

class CountingProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit CountingProviderAdapter(std::shared_ptr<ImageSequenceProviderSessionFactory> factory,
        ImageSequenceProviderMetadata knownMetadata = {},
        CapabilitySupport timedPlaybackSupport = CapabilitySupport::Unavailable,
        CapabilitySupport frameSeekSupport = CapabilitySupport::Unavailable,
        CapabilitySupport positionSeekSupport = CapabilitySupport::Unavailable,
        QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
        , m_knownMetadata(std::move(knownMetadata))
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return m_factory;
    }

    ImageSequenceProviderMetadata knownMetadata() const override { return m_knownMetadata; }

    CapabilitySupport timedPlaybackCapability() const override { return m_timedPlaybackSupport; }

    CapabilitySupport frameSeekCapability() const override { return m_frameSeekSupport; }

    CapabilitySupport positionSeekCapability() const override { return m_positionSeekSupport; }

private:
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_factory;
    ImageSequenceProviderMetadata m_knownMetadata;
    CapabilitySupport m_timedPlaybackSupport = CapabilitySupport::Unavailable;
    CapabilitySupport m_frameSeekSupport = CapabilitySupport::Unavailable;
    CapabilitySupport m_positionSeekSupport = CapabilitySupport::Unavailable;
};

class AffinityProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit AffinityProviderSession(const std::shared_ptr<QThread*>& metadataRequestThread,
        const std::shared_ptr<QThread*>& frameRequestThread,
        const std::shared_ptr<QThread*>& playbackRequestThread,
        const std::shared_ptr<QThread*>& cancelRequestThread,
        const std::shared_ptr<QThread*>& closeThread, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestThread(metadataRequestThread)
        , m_frameRequestThread(frameRequestThread)
        , m_playbackRequestThread(playbackRequestThread)
        , m_cancelRequestThread(cancelRequestThread)
        , m_closeThread(closeThread)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken& token) override
    {
        *m_metadataRequestThread = QThread::currentThread();
        m_lastMetadataToken = token;
    }

    void requestFrame(const ImageSequenceProviderRequestToken& token, int) override
    {
        *m_frameRequestThread = QThread::currentThread();
        m_lastFrameToken = token;
    }

    void requestPlayback(const ImageSequenceProviderRequestToken& token, int, int) override
    {
        *m_playbackRequestThread = QThread::currentThread();
        m_lastPlaybackToken = token;
    }

    void cancelRequest(const ImageSequenceProviderRequestToken&) override
    {
        *m_cancelRequestThread = QThread::currentThread();
    }

    void close() override { *m_closeThread = QThread::currentThread(); }

    ImageSequenceProviderRequestToken lastMetadataToken() const { return m_lastMetadataToken; }

    ImageSequenceProviderRequestToken lastFrameToken() const { return m_lastFrameToken; }

    ImageSequenceProviderRequestToken lastPlaybackToken() const { return m_lastPlaybackToken; }

private:
    std::shared_ptr<QThread*> m_metadataRequestThread;
    std::shared_ptr<QThread*> m_frameRequestThread;
    std::shared_ptr<QThread*> m_playbackRequestThread;
    std::shared_ptr<QThread*> m_cancelRequestThread;
    std::shared_ptr<QThread*> m_closeThread;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
    ImageSequenceProviderRequestToken m_lastFrameToken;
    ImageSequenceProviderRequestToken m_lastPlaybackToken;
};

class AffinityProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit AffinityProviderSessionFactory(QThread* thread,
        const std::shared_ptr<QThread*>& metadataRequestThread,
        const std::shared_ptr<QThread*>& frameRequestThread,
        const std::shared_ptr<QThread*>& playbackRequestThread,
        const std::shared_ptr<QThread*>& cancelRequestThread,
        const std::shared_ptr<QThread*>& closeThread)
        : m_thread(thread)
        , m_metadataRequestThread(metadataRequestThread)
        , m_frameRequestThread(frameRequestThread)
        , m_playbackRequestThread(playbackRequestThread)
        , m_cancelRequestThread(cancelRequestThread)
        , m_closeThread(closeThread)
    {
    }

    ImageSequenceProviderSession* createSession(QObject*) override
    {
        auto* session = new AffinityProviderSession(m_metadataRequestThread, m_frameRequestThread,
            m_playbackRequestThread, m_cancelRequestThread, m_closeThread);
        session->moveToThread(m_thread);
        m_lastSession = session;
        return session;
    }

    AffinityProviderSession* lastSession() const { return m_lastSession; }

private:
    QThread* m_thread = nullptr;
    std::shared_ptr<QThread*> m_metadataRequestThread;
    std::shared_ptr<QThread*> m_frameRequestThread;
    std::shared_ptr<QThread*> m_playbackRequestThread;
    std::shared_ptr<QThread*> m_cancelRequestThread;
    std::shared_ptr<QThread*> m_closeThread;
    QPointer<AffinityProviderSession> m_lastSession;
};

class FailingProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit FailingProviderSessionFactory(const std::shared_ptr<int>& sessionCount)
        : m_sessionCount(sessionCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject*) override
    {
        ++*m_sessionCount;
        return nullptr;
    }

private:
    std::shared_ptr<int> m_sessionCount;
};

class CancellingAcknowledgementProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit CancellingAcknowledgementProviderSession(
        const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount,
        const std::shared_ptr<int>& cancelRequestCount, const std::shared_ptr<int>& closeCount,
        QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_cancelRequestCount(cancelRequestCount)
        , m_closeCount(closeCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken& token) override
    {
        m_lastMetadataToken = token;
        ++*m_metadataRequestCount;
    }

    void requestFrame(const ImageSequenceProviderRequestToken&, int) override
    {
        ++*m_frameRequestCount;
    }

    void cancelRequest(const ImageSequenceProviderRequestToken& token) override
    {
        ++*m_cancelRequestCount;
        emit providerCancelled(token, QStringLiteral("request cleanup complete"));
    }

    void close() override { ++*m_closeCount; }

    ImageSequenceProviderRequestToken lastMetadataToken() const { return m_lastMetadataToken; }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<int> m_closeCount;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
};

class CancellingAcknowledgementProviderSessionFactory final
    : public ImageSequenceProviderSessionFactory
{
public:
    explicit CancellingAcknowledgementProviderSessionFactory(
        const std::shared_ptr<int>& sessionCount, const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount,
        const std::shared_ptr<int>& cancelRequestCount, const std::shared_ptr<int>& closeCount)
        : m_sessionCount(sessionCount)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
        , m_cancelRequestCount(cancelRequestCount)
        , m_closeCount(closeCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        ++*m_sessionCount;
        auto* session = new CancellingAcknowledgementProviderSession(m_metadataRequestCount,
            m_frameRequestCount, m_cancelRequestCount, m_closeCount, parent);
        m_lastSession = session;
        return session;
    }

    CancellingAcknowledgementProviderSession* lastSession() const { return m_lastSession; }

private:
    std::shared_ptr<int> m_sessionCount;
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<int> m_closeCount;
    QPointer<CancellingAcknowledgementProviderSession> m_lastSession;
};

class NullSessionFactoryProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    explicit NullSessionFactoryProviderAdapter(QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return {};
    }
};

class SynchronousMetadataProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousMetadataProviderSession(const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken& token) override
    {
        ++*m_metadataRequestCount;
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    }

    void requestFrame(const ImageSequenceProviderRequestToken&, int) override
    {
        ++*m_frameRequestCount;
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousMetadataProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousMetadataProviderSessionFactory(
        const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new SynchronousMetadataProviderSession(
            m_metadataRequestCount, m_frameRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousFrameProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousFrameProviderSession(
        const std::shared_ptr<int>& frameRequestCount, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_frameRequestCount(frameRequestCount)
    {
        QImage image(16, 8, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        m_frame = std::make_unique<ImageFrame>(image);
    }

    void requestMetadata(const ImageSequenceProviderRequestToken&) override
    {
        QFAIL("complete construction metadata should not request runtime metadata");
    }

    void requestFrame(const ImageSequenceProviderRequestToken& token, int) override
    {
        ++*m_frameRequestCount;
        emit imageFrameReady(token, m_frame.get());
    }

private:
    std::shared_ptr<int> m_frameRequestCount;
    std::unique_ptr<ImageFrame> m_frame;
};

class SynchronousFrameProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousFrameProviderSessionFactory(const std::shared_ptr<int>& frameRequestCount)
        : m_frameRequestCount(frameRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new SynchronousFrameProviderSession(m_frameRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousFailureProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousFailureProviderSession(
        const std::shared_ptr<int>& metadataRequestCount, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken& token) override
    {
        ++*m_metadataRequestCount;
        emit providerFailed(token, QStringLiteral("metadata failed synchronously"));
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class SynchronousFailureProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousFailureProviderSessionFactory(
        const std::shared_ptr<int>& metadataRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new SynchronousFailureProviderSession(m_metadataRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class SynchronousUnsupportedProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SynchronousUnsupportedProviderSession(
        const std::shared_ptr<int>& metadataRequestCount, QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_metadataRequestCount(metadataRequestCount)
    {
    }

    void requestMetadata(const ImageSequenceProviderRequestToken& token) override
    {
        ++*m_metadataRequestCount;
        emit providerUnsupported(token, QStringLiteral("metadata unsupported synchronously"));
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class SynchronousUnsupportedProviderSessionFactory final
    : public ImageSequenceProviderSessionFactory
{
public:
    explicit SynchronousUnsupportedProviderSessionFactory(
        const std::shared_ptr<int>& metadataRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        return new SynchronousUnsupportedProviderSession(m_metadataRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

void drainQueuedProviderResults()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}

void emitTimedProviderFrameReady(CountingProviderSession* session,
    const ImageSequenceProviderRequestToken& token, ImageFrame* frame, int frameIndex,
    int frameStartPosition)
{
    emit session->imageFrameWithMetadataReady(token, frame,
        ImageSequenceProviderFrameMetadata::timedFrame(frameIndex, frameStartPosition));
    drainQueuedProviderResults();
}

void emitTimedProviderFrameReady(
    CountingProviderSession* session, ImageFrame* frame, int frameIndex, int frameStartPosition)
{
    emitTimedProviderFrameReady(
        session, session->lastFrameToken(), frame, frameIndex, frameStartPosition);
}

}
