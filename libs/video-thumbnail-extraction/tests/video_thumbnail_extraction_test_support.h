// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_TEST_SUPPORT_H
#define KIRIVIEW_VIDEO_THUMBNAIL_EXTRACTION_TEST_SUPPORT_H

#include "videothumbnailextraction_p.h"

#include <QCoreApplication>
#include <QEvent>

#include <memory>
#include <utility>
#include <vector>

namespace kiriview::test {

using detail::VideoThumbnailBackendCallbacks;
using detail::VideoThumbnailBackendError;
using detail::VideoThumbnailBackendFrame;
using detail::VideoThumbnailBackendMediaFacts;
using detail::VideoThumbnailBackendMediaStatus;
using detail::VideoThumbnailEmbeddedImages;

struct BackendProbe;

class FakeVideoThumbnailBackend final : public detail::VideoThumbnailBackend
{
public:
    explicit FakeVideoThumbnailBackend(std::shared_ptr<BackendProbe> probe);
    ~FakeVideoThumbnailBackend() override;

    void setCallbacks(VideoThumbnailBackendCallbacks callbacks) override;
    void setSource(const QUrl& sourceUrl) override;
    void play() override;
    void pause() override;
    void stop() noexcept override;
    void setPosition(qint64 positionMsec) override;

    void emitMediaFacts(VideoThumbnailBackendMediaFacts facts);
    void emitFrame(QImage image);
    void emitFrame(VideoThumbnailBackendFrame frame);
    void emitMetadata(VideoThumbnailEmbeddedImages images);
    void emitError(VideoThumbnailBackendError error, QString diagnostic = {});

private:
    std::shared_ptr<BackendProbe> probe_;
    VideoThumbnailBackendCallbacks callbacks_;
};

struct BackendProbe
{
    int creations = 0;
    int destructions = 0;
    int setSourceCalls = 0;
    int playCalls = 0;
    int pauseCalls = 0;
    int stopCalls = 0;
    QUrl sourceUrl;
    std::vector<qint64> positions;
    FakeVideoThumbnailBackend* instance = nullptr;
    bool emitErrorFromStop = false;
};

inline FakeVideoThumbnailBackend::FakeVideoThumbnailBackend(std::shared_ptr<BackendProbe> probe)
    : probe_(std::move(probe))
{
    ++probe_->creations;
    probe_->instance = this;
}

inline FakeVideoThumbnailBackend::~FakeVideoThumbnailBackend()
{
    ++probe_->destructions;
    probe_->instance = nullptr;
}

inline void FakeVideoThumbnailBackend::setCallbacks(VideoThumbnailBackendCallbacks callbacks)
{
    callbacks_ = std::move(callbacks);
}

inline void FakeVideoThumbnailBackend::setSource(const QUrl& sourceUrl)
{
    ++probe_->setSourceCalls;
    probe_->sourceUrl = sourceUrl;
}

inline void FakeVideoThumbnailBackend::play() { ++probe_->playCalls; }

inline void FakeVideoThumbnailBackend::pause() { ++probe_->pauseCalls; }

inline void FakeVideoThumbnailBackend::stop() noexcept
{
    ++probe_->stopCalls;
    if (probe_->emitErrorFromStop && callbacks_.errorOccurred) {
        callbacks_.errorOccurred(
            VideoThumbnailBackendError::Other, QStringLiteral("reentrant stop error"));
    }
}

inline void FakeVideoThumbnailBackend::setPosition(qint64 positionMsec)
{
    probe_->positions.push_back(positionMsec);
}

inline void FakeVideoThumbnailBackend::emitMediaFacts(VideoThumbnailBackendMediaFacts facts)
{
    if (callbacks_.mediaFactsChanged) {
        callbacks_.mediaFactsChanged(facts);
    }
}

inline void FakeVideoThumbnailBackend::emitFrame(QImage image)
{
    const QSize pixelSize = image.size();
    emitFrame(VideoThumbnailBackendFrame(
        pixelSize, [image = std::move(image)]() mutable { return std::move(image); }));
}

inline void FakeVideoThumbnailBackend::emitFrame(VideoThumbnailBackendFrame frame)
{
    if (callbacks_.frameAvailable) {
        callbacks_.frameAvailable(std::move(frame));
    }
}

inline void FakeVideoThumbnailBackend::emitMetadata(VideoThumbnailEmbeddedImages images)
{
    if (callbacks_.metadataAvailable) {
        callbacks_.metadataAvailable(std::move(images));
    }
}

inline void FakeVideoThumbnailBackend::emitError(
    VideoThumbnailBackendError error, QString diagnostic)
{
    if (callbacks_.errorOccurred) {
        callbacks_.errorOccurred(error, std::move(diagnostic));
    }
}

struct DeadlineProbe;

class FakeVideoThumbnailDeadline final : public detail::VideoThumbnailDeadline
{
public:
    explicit FakeVideoThumbnailDeadline(std::shared_ptr<DeadlineProbe> probe);
    ~FakeVideoThumbnailDeadline() override;

    void start(std::chrono::milliseconds interval, std::function<void()> expired) override;
    void stop() noexcept override;

private:
    std::shared_ptr<DeadlineProbe> probe_;
};

struct DeadlineProbe
{
    int creations = 0;
    int destructions = 0;
    int startCalls = 0;
    int stopCalls = 0;
    std::chrono::milliseconds interval {};
    std::function<void()> expired;

    void fire()
    {
        if (expired) {
            expired();
        }
    }
};

inline FakeVideoThumbnailDeadline::FakeVideoThumbnailDeadline(std::shared_ptr<DeadlineProbe> probe)
    : probe_(std::move(probe))
{
    ++probe_->creations;
}

inline FakeVideoThumbnailDeadline::~FakeVideoThumbnailDeadline() { ++probe_->destructions; }

inline void FakeVideoThumbnailDeadline::start(
    std::chrono::milliseconds interval, std::function<void()> expired)
{
    ++probe_->startCalls;
    probe_->interval = interval;
    probe_->expired = std::move(expired);
}

inline void FakeVideoThumbnailDeadline::stop() noexcept
{
    ++probe_->stopCalls;
    probe_->expired = {};
}

struct ExtractionHarness
{
    std::shared_ptr<BackendProbe> backend = std::make_shared<BackendProbe>();
    std::shared_ptr<DeadlineProbe> deadline = std::make_shared<DeadlineProbe>();

    [[nodiscard]] auto dependencies() const -> detail::VideoThumbnailRuntimeDependencies
    {
        return {
            [probe = backend]() -> std::unique_ptr<detail::VideoThumbnailBackend> {
                return std::make_unique<FakeVideoThumbnailBackend>(probe);
            },
            [probe = deadline]() -> std::unique_ptr<detail::VideoThumbnailDeadline> {
                return std::make_unique<FakeVideoThumbnailDeadline>(probe);
            },
        };
    }
};

inline void drainQueuedCalls()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    QCoreApplication::processEvents();
}

inline auto validRequest(int maximumLongEdge = 128) -> VideoThumbnailExtractionRequest
{
    return { QUrl::fromLocalFile(QStringLiteral("/tmp/kiriview-video.mp4")), maximumLongEdge };
}

} // namespace kiriview::test

#endif
