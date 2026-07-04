#pragma once

#include "imageviewport_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QTest>

namespace {

class PlaybackFallbackSession final : public ImageSequenceProviderSession
{
public:
    void requestMetadata(ImageSequenceProviderRequestToken) override { }

    void requestFrame(ImageSequenceProviderRequestToken token, int frame) override
    {
        lastFrameToken = token;
        lastFrame = frame;
        ++frameRequestCount;
    }

    ImageSequenceProviderRequestToken lastFrameToken;
    int lastFrame = -1;
    int frameRequestCount = 0;
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

    void requestMetadata(ImageSequenceProviderRequestToken token) override
    {
        ++*m_metadataRequestCount;
        emit metadataReady(token, ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
    }

    void requestFrame(ImageSequenceProviderRequestToken, int) override { ++*m_frameRequestCount; }

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

    void requestMetadata(ImageSequenceProviderRequestToken) override
    {
        QFAIL("complete construction metadata should not request runtime metadata");
    }

    void requestFrame(ImageSequenceProviderRequestToken token, int) override
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

    void requestMetadata(ImageSequenceProviderRequestToken token) override
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

    void requestMetadata(ImageSequenceProviderRequestToken token) override
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

}
