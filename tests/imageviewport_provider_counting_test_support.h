#pragma once

#include "imageviewport_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QTest>

namespace {

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
        const std::shared_ptr<quint64>& lastCancelledTokenId = {},
        const std::shared_ptr<int>& positionRequestCount = {},
        const std::shared_ptr<int>& lastPositionFrame = {},
        const std::shared_ptr<int>& lastRequestedPosition = {}, QObject* parent = nullptr)
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
        , m_positionRequestCount(positionRequestCount)
        , m_lastPositionFrame(lastPositionFrame)
        , m_lastRequestedPosition(lastRequestedPosition)
    {
    }

    void requestMetadata(ImageSequenceProviderRequestToken token) override
    {
        m_lastMetadataToken = token;
        ++*m_metadataRequestCount;
    }

    void requestFrame(ImageSequenceProviderRequestToken token, int frame) override
    {
        m_lastFrameToken = token;
        *m_lastRequestedFrame = frame;
        ++*m_frameRequestCount;
    }

    void requestPosition(
        ImageSequenceProviderRequestToken token, int resolvedFrame, int requestedPosition) override
    {
        if (!m_positionRequestCount && !m_lastPositionFrame && !m_lastRequestedPosition) {
            ImageSequenceProviderSession::requestPosition(token, resolvedFrame, requestedPosition);
            return;
        }
        m_lastPositionToken = token;
        if (m_positionRequestCount) {
            ++*m_positionRequestCount;
        }
        if (m_lastPositionFrame) {
            *m_lastPositionFrame = resolvedFrame;
        }
        if (m_lastRequestedPosition) {
            *m_lastRequestedPosition = requestedPosition;
        }
    }

    void requestPlayback(ImageSequenceProviderRequestToken token, int frame, int position) override
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

    void cancelRequest(ImageSequenceProviderRequestToken token) override
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

    ImageSequenceProviderRequestToken lastPositionToken() const { return m_lastPositionToken; }

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
    std::shared_ptr<int> m_positionRequestCount;
    std::shared_ptr<int> m_lastPositionFrame;
    std::shared_ptr<int> m_lastRequestedPosition;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
    ImageSequenceProviderRequestToken m_lastFrameToken;
    ImageSequenceProviderRequestToken m_lastPositionToken;
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
        const std::shared_ptr<quint64>& lastCancelledTokenId = {},
        const std::shared_ptr<int>& positionRequestCount = {},
        const std::shared_ptr<int>& lastPositionFrame = {},
        const std::shared_ptr<int>& lastRequestedPosition = {})
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
        , m_positionRequestCount(positionRequestCount)
        , m_lastPositionFrame(lastPositionFrame)
        , m_lastRequestedPosition(lastRequestedPosition)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent) override
    {
        ++*m_sessionCount;
        CountingProviderSession* session
            = new CountingProviderSession(m_metadataRequestCount, m_frameRequestCount,
                m_lastRequestedFrame, m_closeCount, m_playbackRequestCount, m_lastPlaybackFrame,
                m_lastPlaybackPosition, m_cancelRequestCount, m_lastCancelledTokenId,
                m_positionRequestCount, m_lastPositionFrame, m_lastRequestedPosition, parent);
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
    std::shared_ptr<int> m_positionRequestCount;
    std::shared_ptr<int> m_lastPositionFrame;
    std::shared_ptr<int> m_lastRequestedPosition;
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
        ImageSequenceProviderThreadingContract threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound,
        QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
        , m_knownMetadata(std::move(knownMetadata))
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
        , m_threadingContract(threadingContract)
    {
    }

    explicit CountingProviderAdapter(std::shared_ptr<ImageSequenceProviderSessionFactory> factory,
        ImageSequenceProviderKnownFacts knownFacts,
        CapabilitySupport timedPlaybackSupport = CapabilitySupport::Unavailable,
        CapabilitySupport frameSeekSupport = CapabilitySupport::Unavailable,
        CapabilitySupport positionSeekSupport = CapabilitySupport::Unavailable,
        ImageSequenceProviderThreadingContract threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound,
        QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_factory(std::move(factory))
        , m_knownFacts(std::move(knownFacts))
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
        , m_threadingContract(threadingContract)
    {
    }

    std::shared_ptr<ImageSequenceProviderSessionFactory> sessionFactory() const override
    {
        return m_factory;
    }

    ImageSequenceProviderMetadata knownMetadata() const override { return m_knownMetadata; }

    ImageSequenceProviderKnownFacts knownFacts() const override
    {
        if (m_knownFacts.isSpecified()) {
            return m_knownFacts;
        }
        return ImageSequenceProviderAdapter::knownFacts();
    }

    CapabilitySupport timedPlaybackCapability() const override { return m_timedPlaybackSupport; }

    CapabilitySupport frameSeekCapability() const override { return m_frameSeekSupport; }

    CapabilitySupport positionSeekCapability() const override { return m_positionSeekSupport; }

    ImageSequenceProviderThreadingContract threadingContract() const override
    {
        return m_threadingContract;
    }

private:
    std::shared_ptr<ImageSequenceProviderSessionFactory> m_factory;
    ImageSequenceProviderMetadata m_knownMetadata;
    ImageSequenceProviderKnownFacts m_knownFacts;
    CapabilitySupport m_timedPlaybackSupport = CapabilitySupport::Unavailable;
    CapabilitySupport m_frameSeekSupport = CapabilitySupport::Unavailable;
    CapabilitySupport m_positionSeekSupport = CapabilitySupport::Unavailable;
    ImageSequenceProviderThreadingContract m_threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
};

void drainQueuedProviderResults()
{
    // Allowlist: use only for queued provider callback delivery, Qt-affinity cleanup, or event-loop
    // handoff cases that the synchronous provider executor must not replace.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}

void emitTimedProviderFrameReady(CountingProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageFrame* frame, int frameIndex,
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
