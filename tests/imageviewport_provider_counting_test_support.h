#pragma once

#include "imageviewport_provider_event_test_support.h"
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
        const std::shared_ptr<ImageSequenceProviderRequestToken>& lastCancelledToken = {},
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
        , m_lastCancelledTokenSink(lastCancelledToken)
        , m_positionRequestCount(positionRequestCount)
        , m_lastPositionFrame(lastPositionFrame)
        , m_lastRequestedPosition(lastRequestedPosition)
    {
    }

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Frame
            || request.kind() == ImageSequenceProviderRequestKind::Position
            || request.kind() == ImageSequenceProviderRequestKind::Playback) {
            m_lastFrameDemand = request.demand();
        }
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
            m_lastMetadataToken = request.token();
            ++*m_metadataRequestCount;
            break;
        case ImageSequenceProviderRequestKind::Frame:
            recordFrameRequest(request.token(), request.frame());
            break;
        case ImageSequenceProviderRequestKind::Position:
            if (!m_positionRequestCount && !m_lastPositionFrame && !m_lastRequestedPosition) {
                recordFrameRequest(request.token(), request.resolvedFrame());
                break;
            }
            m_lastPositionToken = request.token();
            if (m_positionRequestCount) {
                ++*m_positionRequestCount;
            }
            if (m_lastPositionFrame) {
                *m_lastPositionFrame = request.resolvedFrame();
            }
            if (m_lastRequestedPosition) {
                *m_lastRequestedPosition = request.requestedPosition();
            }
            break;
        case ImageSequenceProviderRequestKind::Playback:
            if (m_playbackRequestCount) {
                ++*m_playbackRequestCount;
            }
            if (m_lastPlaybackFrame) {
                *m_lastPlaybackFrame = request.frame();
            }
            if (m_lastPlaybackPosition) {
                *m_lastPlaybackPosition = request.requestedPosition();
            }
            recordFrameRequest(request.token(), request.frame());
            break;
        case ImageSequenceProviderRequestKind::Cancel:
            for (ImageSequenceProviderRequestToken token : request.tokens()) {
                m_lastCancelledToken = token;
                if (m_cancelRequestCount) {
                    ++*m_cancelRequestCount;
                }
                if (m_lastCancelledTokenSink) {
                    *m_lastCancelledTokenSink = token;
                }
            }
            break;
        case ImageSequenceProviderRequestKind::Close:
            ++*m_closeCount;
            break;
        }
    }

    ImageSequenceProviderRequestToken lastMetadataToken() const { return m_lastMetadataToken; }

    ImageSequenceProviderRequestToken lastFrameToken() const { return m_lastFrameToken; }

    ImageSequenceProviderRequestToken lastPositionToken() const { return m_lastPositionToken; }

    ImageSequenceProviderRequestToken lastCancelledToken() const { return m_lastCancelledToken; }

    ImageSequenceProviderDisplayDemand lastFrameDemand() const { return m_lastFrameDemand; }

private:
    void recordFrameRequest(ImageSequenceProviderRequestToken token, int frame)
    {
        m_lastFrameToken = token;
        *m_lastRequestedFrame = frame;
        ++*m_frameRequestCount;
    }

    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
    std::shared_ptr<int> m_lastRequestedFrame;
    std::shared_ptr<int> m_closeCount;
    std::shared_ptr<int> m_playbackRequestCount;
    std::shared_ptr<int> m_lastPlaybackFrame;
    std::shared_ptr<int> m_lastPlaybackPosition;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<ImageSequenceProviderRequestToken> m_lastCancelledTokenSink;
    std::shared_ptr<int> m_positionRequestCount;
    std::shared_ptr<int> m_lastPositionFrame;
    std::shared_ptr<int> m_lastRequestedPosition;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
    ImageSequenceProviderRequestToken m_lastFrameToken;
    ImageSequenceProviderRequestToken m_lastPositionToken;
    ImageSequenceProviderRequestToken m_lastCancelledToken;
    ImageSequenceProviderDisplayDemand m_lastFrameDemand;
};

class CountingProviderSessionFactory final
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
        const std::shared_ptr<ImageSequenceProviderRequestToken>& lastCancelledToken = {},
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
        , m_lastCancelledToken(lastCancelledToken)
        , m_positionRequestCount(positionRequestCount)
        , m_lastPositionFrame(lastPositionFrame)
        , m_lastRequestedPosition(lastRequestedPosition)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
    {
        ++*m_sessionCount;
        CountingProviderSession* session = new CountingProviderSession(m_metadataRequestCount,
            m_frameRequestCount, m_lastRequestedFrame, m_closeCount, m_playbackRequestCount,
            m_lastPlaybackFrame, m_lastPlaybackPosition, m_cancelRequestCount, m_lastCancelledToken,
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
    std::shared_ptr<ImageSequenceProviderRequestToken> m_lastCancelledToken;
    std::shared_ptr<int> m_positionRequestCount;
    std::shared_ptr<int> m_lastPositionFrame;
    std::shared_ptr<int> m_lastRequestedPosition;
    QPointer<CountingProviderSession> m_lastSession;
    QList<QPointer<CountingProviderSession>> m_sessions;
};

class CountingProviderAdapter final : public ImageSequenceProviderAdapter
{
public:
    template <typename Factory>
    explicit CountingProviderAdapter(std::shared_ptr<Factory> factory,
        ImageSequenceProviderMetadata knownMetadata = {},
        ImageViewportCapabilitySupport timedPlaybackSupport
        = ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport frameSeekSupport
        = ImageViewportCapabilitySupport::Unavailable,
        ImageViewportCapabilitySupport positionSeekSupport
        = ImageViewportCapabilitySupport::Unavailable,
        ImageSequenceProviderThreadingContract threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound,
        QObject* parent = nullptr)
        : ImageSequenceProviderAdapter(parent)
        , m_sessionFactory([factory = std::move(factory)]() {
            ImageSequenceProviderSession* session = factory->createSession(nullptr);
            return session ? ImageSequenceProviderSessionFactoryResult::created(session)
                           : ImageSequenceProviderSessionFactoryResult::failed(
                                 QStringLiteral("session creation failed"));
        })
        , m_knownMetadata(std::move(knownMetadata))
        , m_timedPlaybackSupport(timedPlaybackSupport)
        , m_frameSeekSupport(frameSeekSupport)
        , m_positionSeekSupport(positionSeekSupport)
        , m_threadingContract(threadingContract)
    {
    }

    ImageSequenceProviderDescriptor descriptor() const override
    {
        ImageSequenceProviderMetadata metadata = m_knownMetadata;
        metadata.setTimedPlaybackSupport(m_timedPlaybackSupport);
        metadata.setFrameSeekSupport(m_frameSeekSupport);
        metadata.setPositionSeekSupport(m_positionSeekSupport);
        if (m_hasAuthoredAnimationFacts) {
            metadata.setAuthoredAnimationFacts(m_authoredAnimationFacts);
        }
        return ImageSequenceProviderDescriptor(metadata, m_threadingContract, m_sessionFactory);
    }

    void setAuthoredAnimationFacts(ImageSequenceAuthoredAnimationFacts authoredAnimationFacts)
    {
        m_hasAuthoredAnimationFacts = true;
        m_authoredAnimationFacts = authoredAnimationFacts;
    }

private:
    ImageSequenceProviderSessionFactory m_sessionFactory;
    ImageSequenceProviderMetadata m_knownMetadata;
    ImageViewportCapabilitySupport m_timedPlaybackSupport
        = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport m_frameSeekSupport = ImageViewportCapabilitySupport::Unavailable;
    ImageViewportCapabilitySupport m_positionSeekSupport
        = ImageViewportCapabilitySupport::Unavailable;
    bool m_hasAuthoredAnimationFacts = false;
    ImageSequenceAuthoredAnimationFacts m_authoredAnimationFacts;
    ImageSequenceProviderThreadingContract m_threadingContract
        = ImageSequenceProviderThreadingContract::AffinityBound;
};

void drainQueuedProviderResults()
{
    // Allowlist: prefer useSynchronousProviderEventDeliveryForTest for protocol-only tests.
    // Use this only for queued callback delivery, Qt-affinity cleanup, or event-loop handoff
    // cases that the synchronous provider seams must not replace.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}

void emitTimedProviderFrameReady(CountingProviderSession* session,
    ImageSequenceProviderRequestToken token, ImageFrame* frame, int frameIndex,
    int frameStartPosition, int frameDuration = -1)
{
    const int resolvedFrameDuration
        = frameDuration > 0 ? frameDuration : (frameStartPosition == 0 ? 100 : 250);
    emitProviderFrameReady(session, token, frame,
        ImageSequenceProviderFrameEnvelope::timedFrame(
            frameIndex, frameStartPosition, resolvedFrameDuration));
    drainQueuedProviderResults();
}

void emitTimedProviderFrameReady(CountingProviderSession* session, ImageFrame* frame,
    int frameIndex, int frameStartPosition, int frameDuration = -1)
{
    emitTimedProviderFrameReady(
        session, session->lastFrameToken(), frame, frameIndex, frameStartPosition, frameDuration);
}

}
