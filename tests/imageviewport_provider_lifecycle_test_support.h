#pragma once

#include "imageviewport_provider_event_test_support.h"
#include "imageviewport_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QTest>

namespace {

class SlowCleanupProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit SlowCleanupProviderSession(const std::shared_ptr<int>& cancelRequestCount,
        const std::shared_ptr<int>& closeCount, int cleanupDelayMilliseconds,
        QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , m_cancelRequestCount(cancelRequestCount)
        , m_closeCount(closeCount)
        , m_cleanupDelayMilliseconds(cleanupDelayMilliseconds)
    {
    }

    void request(const ImageSequenceProviderRequest& request) override
    {
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
            m_lastMetadataToken = request.token();
            break;
        case ImageSequenceProviderRequestKind::Cancel:
            *m_cancelRequestCount += request.tokens().size();
            break;
        case ImageSequenceProviderRequestKind::Close:
            QTest::qSleep(m_cleanupDelayMilliseconds);
            ++*m_closeCount;
            break;
        default:
            break;
        }
    }

    ImageSequenceProviderRequestToken lastMetadataToken() const { return m_lastMetadataToken; }

private:
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<int> m_closeCount;
    int m_cleanupDelayMilliseconds = 0;
    ImageSequenceProviderRequestToken m_lastMetadataToken;
};

class SlowCleanupProviderSessionFactory final : public ImageSequenceProviderSessionFactory
{
public:
    explicit SlowCleanupProviderSessionFactory(QThread* thread,
        const std::shared_ptr<int>& cancelRequestCount, const std::shared_ptr<int>& closeCount,
        int cleanupDelayMilliseconds)
        : m_thread(thread)
        , m_cancelRequestCount(cancelRequestCount)
        , m_closeCount(closeCount)
        , m_cleanupDelayMilliseconds(cleanupDelayMilliseconds)
    {
    }

    ImageSequenceProviderSession* createSession(QObject*) override
    {
        auto* session = new SlowCleanupProviderSession(
            m_cancelRequestCount, m_closeCount, m_cleanupDelayMilliseconds);
        session->moveToThread(m_thread);
        m_lastSession = session;
        return session;
    }

    SlowCleanupProviderSession* lastSession() const { return m_lastSession; }

private:
    QThread* m_thread = nullptr;
    std::shared_ptr<int> m_cancelRequestCount;
    std::shared_ptr<int> m_closeCount;
    int m_cleanupDelayMilliseconds = 0;
    QPointer<SlowCleanupProviderSession> m_lastSession;
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

    void request(const ImageSequenceProviderRequest& request) override
    {
        switch (request.kind()) {
        case ImageSequenceProviderRequestKind::Metadata:
            m_lastMetadataToken = request.token();
            ++*m_metadataRequestCount;
            break;
        case ImageSequenceProviderRequestKind::Frame:
            ++*m_frameRequestCount;
            break;
        case ImageSequenceProviderRequestKind::Cancel:
            for (ImageSequenceProviderRequestToken token : request.tokens()) {
                ++*m_cancelRequestCount;
                emitProviderCancelled(this, token, QStringLiteral("request cleanup complete"));
            }
            break;
        case ImageSequenceProviderRequestKind::Close:
            ++*m_closeCount;
            break;
        default:
            break;
        }
    }

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

    ImageSequenceProviderDescriptor descriptor() const override { return {}; }

};

}
