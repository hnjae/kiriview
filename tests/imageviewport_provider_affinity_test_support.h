#pragma once

#include "imageviewport_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QTest>

namespace {

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

    void request(const ImageSequenceProviderRequest& request) override
    {
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
            *m_metadataRequestThread = QThread::currentThread();
            m_lastMetadataToken = request.token();
            break;
        case ImageSequenceProviderRequestKind::Frame:
        case ImageSequenceProviderRequestKind::Position:
            *m_frameRequestThread = QThread::currentThread();
            m_lastFrameToken = request.token();
            break;
        case ImageSequenceProviderRequestKind::Playback:
            *m_playbackRequestThread = QThread::currentThread();
            m_lastPlaybackToken = request.token();
            break;
        case ImageSequenceProviderRequestKind::Cancel:
            *m_cancelRequestThread = QThread::currentThread();
            break;
        case ImageSequenceProviderRequestKind::Close:
            *m_closeThread = QThread::currentThread();
            break;
        }
    }

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

class AffinityProviderSessionFactory final
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

    ImageSequenceProviderSession* createSession(QObject*)
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

}
