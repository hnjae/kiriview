// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "qtmultimediathumbnailbackend_p.h"

#include <QChronoTimer>
#include <QDeadlineTimer>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QVariant>
#include <QVideoFrame>
#include <QVideoSink>

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>

namespace {

using kiriview::detail::VideoThumbnailBackendCallbacks;
using kiriview::detail::VideoThumbnailBackendFrame;
using kiriview::detail::VideoThumbnailBackendMediaFacts;

class QtVideoThumbnailBackend final : public kiriview::detail::VideoThumbnailBackend
{
public:
    explicit QtVideoThumbnailBackend(kiriview::detail::QtVideoThumbnailBackendResources resources)
        : player_(std::move(resources.player))
        , sink_(std::move(resources.sink))
    {
        Q_ASSERT(player_ != nullptr);
        Q_ASSERT(sink_ != nullptr);

        player_->setAudioOutput(nullptr);
        player_->setVideoSink(sink_.get());

        QObject::connect(sink_.get(), &QVideoSink::videoFrameChanged, sink_.get(),
            [this](const QVideoFrame& frame) {
                publishMetadata();
                if (callbacks_.frameAvailable) {
                    callbacks_.frameAvailable(frame.isValid()
                            ? VideoThumbnailBackendFrame(
                                  frame.size(), [frame]() { return frame.toImage(); })
                            : VideoThumbnailBackendFrame {});
                }
            });
        QObject::connect(player_.get(), &QMediaPlayer::metaDataChanged, player_.get(),
            [this]() { publishMetadata(); });
        QObject::connect(player_.get(), &QMediaPlayer::mediaStatusChanged, player_.get(),
            [this]() { publishMediaFacts(); });
        QObject::connect(player_.get(), &QMediaPlayer::durationChanged, player_.get(),
            [this]() { publishMediaFacts(); });
        QObject::connect(player_.get(), &QMediaPlayer::seekableChanged, player_.get(),
            [this]() { publishMediaFacts(); });
        QObject::connect(player_.get(), &QMediaPlayer::hasVideoChanged, player_.get(),
            [this]() { publishMediaFacts(); });
        QObject::connect(player_.get(), &QMediaPlayer::positionChanged, player_.get(),
            [this](qint64 positionMsec) {
                publishMetadata();
                if (callbacks_.positionChanged) {
                    callbacks_.positionChanged(positionMsec);
                }
            });
        QObject::connect(player_.get(), &QMediaPlayer::errorOccurred, player_.get(),
            [this](QMediaPlayer::Error error, const QString& diagnostic) {
                if (error == QMediaPlayer::NoError) {
                    return;
                }
                publishMetadata();
                if (callbacks_.errorOccurred) {
                    callbacks_.errorOccurred(
                        kiriview::detail::projectQtVideoThumbnailError(error), diagnostic);
                }
            });
    }

    void setCallbacks(VideoThumbnailBackendCallbacks callbacks) override
    {
        callbacks_ = std::move(callbacks);
    }

    void setSource(const QUrl& sourceUrl) override
    {
        player_->setSource(sourceUrl);
        publishMetadata();
        publishMediaFacts();
    }

    void play() override { player_->play(); }

    void pause() override { player_->pause(); }

    void stop() noexcept override
    {
        player_->stop();
        player_->setSource({});
        sink_->setVideoFrame({});
    }

    void setPosition(qint64 positionMsec) override
    {
        player_->setPosition(positionMsec);
        if (player_->position() == positionMsec && callbacks_.positionChanged) {
            callbacks_.positionChanged(positionMsec);
        }
    }

private:
    void publishMediaFacts()
    {
        publishMetadata();
        if (player_->mediaStatus() == QMediaPlayer::InvalidMedia) {
            if (player_->error() != QMediaPlayer::NoError && callbacks_.errorOccurred) {
                callbacks_.errorOccurred(
                    kiriview::detail::projectQtVideoThumbnailError(player_->error()),
                    player_->errorString());
            } else {
                QMetaObject::invokeMethod(
                    player_.get(),
                    [this]() {
                        if (player_->mediaStatus() != QMediaPlayer::InvalidMedia) {
                            return;
                        }
                        if (player_->error() != QMediaPlayer::NoError && callbacks_.errorOccurred) {
                            callbacks_.errorOccurred(
                                kiriview::detail::projectQtVideoThumbnailError(player_->error()),
                                player_->errorString());
                            return;
                        }
                        publishCurrentMediaFacts();
                    },
                    Qt::QueuedConnection);
            }
            return;
        }
        publishCurrentMediaFacts();
    }

    void publishCurrentMediaFacts()
    {
        if (callbacks_.mediaFactsChanged) {
            callbacks_.mediaFactsChanged(VideoThumbnailBackendMediaFacts {
                kiriview::detail::projectQtVideoThumbnailMediaStatus(player_->mediaStatus()),
                player_->duration(), player_->isSeekable(), player_->hasVideo() });
        }
    }

    void publishMetadata()
    {
        auto images = kiriview::detail::projectQtVideoThumbnailMetadata(player_->metaData());
        if ((images.coverArt.isNull() && images.thumbnail.isNull())
            || !callbacks_.metadataAvailable) {
            return;
        }
        callbacks_.metadataAvailable(std::move(images));
    }

    std::unique_ptr<QMediaPlayer> player_;
    std::unique_ptr<QVideoSink> sink_;
    VideoThumbnailBackendCallbacks callbacks_;
};

class QtVideoThumbnailDeadline final : public kiriview::detail::VideoThumbnailDeadline
{
public:
    QtVideoThumbnailDeadline()
    {
        timer_.setSingleShot(true);
        timer_.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer_, &QChronoTimer::timeout, &timer_, [this]() {
            if (!expired_) {
                return;
            }
            if (!deadline_.hasExpired()) {
                timer_.setInterval(
                    std::max(deadline_.remainingTimeAsDuration(), std::chrono::nanoseconds(1)));
                timer_.start();
                return;
            }
            auto expired = std::exchange(expired_, {});
            expired();
        });
    }

    void start(std::chrono::milliseconds interval, std::function<void()> expired) override
    {
        expired_ = std::move(expired);
        deadline_ = QDeadlineTimer(interval, Qt::PreciseTimer);
        timer_.setInterval(
            std::max(deadline_.remainingTimeAsDuration(), std::chrono::nanoseconds(1)));
        timer_.start();
    }

    void stop() noexcept override
    {
        timer_.stop();
        expired_ = {};
    }

private:
    QChronoTimer timer_;
    QDeadlineTimer deadline_;
    std::function<void()> expired_;
};

} // namespace

namespace kiriview::detail {

auto projectQtVideoThumbnailMediaStatus(QMediaPlayer::MediaStatus status)
    -> VideoThumbnailBackendMediaStatus
{
    switch (status) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        return VideoThumbnailBackendMediaStatus::Ready;
    case QMediaPlayer::EndOfMedia:
        return VideoThumbnailBackendMediaStatus::EndOfMedia;
    case QMediaPlayer::InvalidMedia:
        return VideoThumbnailBackendMediaStatus::Invalid;
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadingMedia:
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::BufferingMedia:
        return VideoThumbnailBackendMediaStatus::Pending;
    }
    return VideoThumbnailBackendMediaStatus::Pending;
}

auto projectQtVideoThumbnailError(QMediaPlayer::Error error) -> VideoThumbnailBackendError
{
    switch (error) {
    case QMediaPlayer::ResourceError:
        return VideoThumbnailBackendError::Resource;
    case QMediaPlayer::FormatError:
        return VideoThumbnailBackendError::Format;
    case QMediaPlayer::NetworkError:
        return VideoThumbnailBackendError::Network;
    case QMediaPlayer::AccessDeniedError:
        return VideoThumbnailBackendError::AccessDenied;
    case QMediaPlayer::NoError:
        return VideoThumbnailBackendError::Other;
    }
    return VideoThumbnailBackendError::Other;
}

auto projectQtVideoThumbnailMetadata(const QMediaMetaData& metadata) -> VideoThumbnailEmbeddedImages
{
    return {
        metadata.value(QMediaMetaData::CoverArtImage).value<QImage>(),
        metadata.value(QMediaMetaData::ThumbnailImage).value<QImage>(),
    };
}

auto createQtVideoThumbnailBackend(QtVideoThumbnailBackendResources resources)
    -> std::unique_ptr<VideoThumbnailBackend>
{
    return std::make_unique<QtVideoThumbnailBackend>(std::move(resources));
}

auto createQtVideoThumbnailBackend() -> std::unique_ptr<VideoThumbnailBackend>
{
    QtVideoThumbnailBackendResources resources;
    resources.player = std::make_unique<QMediaPlayer>();
    resources.sink = std::make_unique<QVideoSink>();
    return createQtVideoThumbnailBackend(std::move(resources));
}

auto createQtVideoThumbnailDeadline() -> std::unique_ptr<VideoThumbnailDeadline>
{
    return std::make_unique<QtVideoThumbnailDeadline>();
}

} // namespace kiriview::detail
