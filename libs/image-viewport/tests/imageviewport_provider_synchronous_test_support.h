/*
 * SPDX-FileCopyrightText: 2026 KIM Hyunjae
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include "imageviewport_provider_event_test_support.h"
#include "imageviewport_test_support.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QTest>

#include <functional>

namespace {

class PlaybackFallbackSession final : public ImageSequenceProviderSession
{
public:
    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Frame
            || request.kind() == ImageSequenceProviderRequestKind::Playback
            || request.kind() == ImageSequenceProviderRequestKind::Position) {
            lastFrameToken = request.token();
            lastFrame = request.resolvedFrame();
            ++frameRequestCount;
        }
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

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            ++*m_metadataRequestCount;
            emitProviderMetadataReady(
                this, request.token(), ImageSequenceProviderMetadata::still(QSizeF(16.0, 8.0)));
        } else if (request.kind() == ImageSequenceProviderRequestKind::Frame) {
            ++*m_frameRequestCount;
        }
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
    std::shared_ptr<int> m_frameRequestCount;
};

class SynchronousMetadataProviderSessionFactory final
{
public:
    explicit SynchronousMetadataProviderSessionFactory(
        const std::shared_ptr<int>& metadataRequestCount,
        const std::shared_ptr<int>& frameRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
        , m_frameRequestCount(frameRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
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

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            QFAIL("complete construction metadata should not request runtime metadata");
        }
        if (request.kind() == ImageSequenceProviderRequestKind::Frame) {
            ++*m_frameRequestCount;
            emitProviderFrameReady(
                this, request.token(), m_frame.get(), providerStillFrameEnvelope(request.demand()));
        }
    }

private:
    std::shared_ptr<int> m_frameRequestCount;
    std::unique_ptr<ImageFrame> m_frame;
};

class SynchronousFrameProviderSessionFactory final
{
public:
    explicit SynchronousFrameProviderSessionFactory(const std::shared_ptr<int>& frameRequestCount)
        : m_frameRequestCount(frameRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
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

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            ++*m_metadataRequestCount;
            emitProviderFailed(
                this, request.token(), QStringLiteral("metadata failed synchronously"));
        }
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class SynchronousFailureProviderSessionFactory final
{
public:
    explicit SynchronousFailureProviderSessionFactory(
        const std::shared_ptr<int>& metadataRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
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

    void request(const ImageSequenceProviderRequest& request) override
    {
        if (request.kind() == ImageSequenceProviderRequestKind::Metadata) {
            ++*m_metadataRequestCount;
            emitProviderUnsupported(this, request.token(),
                ImageSequenceProviderUnsupportedCause::UnsupportedRequest,
                QStringLiteral("metadata unsupported synchronously"));
        }
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class SynchronousUnsupportedProviderSessionFactory final
{
public:
    explicit SynchronousUnsupportedProviderSessionFactory(
        const std::shared_ptr<int>& metadataRequestCount)
        : m_metadataRequestCount(metadataRequestCount)
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
    {
        return new SynchronousUnsupportedProviderSession(m_metadataRequestCount, parent);
    }

private:
    std::shared_ptr<int> m_metadataRequestCount;
};

class PublicationObservingProviderSession final : public ImageSequenceProviderSession
{
public:
    explicit PublicationObservingProviderSession(
        std::function<void(const ImageSequenceProviderRequest&)> observer,
        QObject* parent = nullptr)
        : ImageSequenceProviderSession(parent)
        , observer(std::move(observer))
    {
    }

    void request(const ImageSequenceProviderRequest& request) override { observer(request); }

private:
    std::function<void(const ImageSequenceProviderRequest&)> observer;
};

class PublicationObservingProviderSessionFactory final
{
public:
    explicit PublicationObservingProviderSessionFactory(
        std::function<void(const ImageSequenceProviderRequest&)> observer)
        : observer(std::move(observer))
    {
    }

    ImageSequenceProviderSession* createSession(QObject* parent)
    {
        return new PublicationObservingProviderSession(observer, parent);
    }

private:
    std::function<void(const ImageSequenceProviderRequest&)> observer;
};

}
