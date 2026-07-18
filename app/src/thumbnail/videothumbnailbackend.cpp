// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "thumbnail/videothumbnailbackend.h"

#include "async/imagecallback.h"

#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QVariant>
#include <QVideoFrame>
#include <QVideoSink>
#include <utility>

namespace {
kiriview::VideoThumbnailBackendMediaStatus mediaStatus(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadedMedia:
    case QMediaPlayer::BufferedMedia:
        return kiriview::VideoThumbnailBackendMediaStatus::Ready;
    case QMediaPlayer::EndOfMedia:
        return kiriview::VideoThumbnailBackendMediaStatus::EndOfMedia;
    case QMediaPlayer::InvalidMedia:
        return kiriview::VideoThumbnailBackendMediaStatus::Invalid;
    case QMediaPlayer::NoMedia:
    case QMediaPlayer::LoadingMedia:
    case QMediaPlayer::StalledMedia:
    case QMediaPlayer::BufferingMedia:
        return kiriview::VideoThumbnailBackendMediaStatus::Pending;
    }
    return kiriview::VideoThumbnailBackendMediaStatus::Pending;
}

class QtVideoThumbnailBackend final : public QObject, public kiriview::VideoThumbnailBackend
{
public:
    explicit QtVideoThumbnailBackend(QObject* parent)
        : QObject(parent)
        , m_player(this)
        , m_sink(this)
    {
        m_player.setVideoSink(&m_sink);
        QObject::connect(
            &m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
                if (frame.isValid()) {
                    publishMetadata();
                    kiriview::invokeIfSet(m_callbacks.frameAvailable, frame.toImage());
                }
            });
        QObject::connect(
            &m_player, &QMediaPlayer::metaDataChanged, this, [this]() { publishMetadata(); });
        QObject::connect(
            &m_player, &QMediaPlayer::mediaStatusChanged, this, [this]() { publishMediaFacts(); });
        QObject::connect(
            &m_player, &QMediaPlayer::durationChanged, this, [this]() { publishMediaFacts(); });
        QObject::connect(
            &m_player, &QMediaPlayer::seekableChanged, this, [this]() { publishMediaFacts(); });
        QObject::connect(&m_player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
            publishMetadata();
            kiriview::invokeIfSet(m_callbacks.positionChanged, position);
        });
        QObject::connect(&m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString& errorString) {
                if (error == QMediaPlayer::NoError) {
                    return;
                }
                publishMetadata();
                kiriview::invokeIfSet(m_callbacks.errorOccurred,
                    errorString.isEmpty() ? QStringLiteral("video thumbnail decode failed")
                                          : errorString);
            });
    }

    void setCallbacks(kiriview::VideoThumbnailBackendCallbacks callbacks) override
    {
        m_callbacks = std::move(callbacks);
    }

    void setSource(const QUrl& sourceUrl) override
    {
        m_player.setSource(sourceUrl);
        publishMetadata();
        publishMediaFacts();
    }
    void play() override { m_player.play(); }
    void pause() override { m_player.pause(); }
    void stop() override { m_player.stop(); }
    void setPosition(qint64 position) override
    {
        m_player.setPosition(position);
        if (m_player.position() == position) {
            publishMetadata();
            kiriview::invokeIfSet(m_callbacks.positionChanged, position);
        }
    }

private:
    void publishMediaFacts()
    {
        publishMetadata();
        kiriview::invokeIfSet(m_callbacks.mediaFactsChanged,
            kiriview::VideoThumbnailBackendMediaFacts {
                mediaStatus(m_player.mediaStatus()), m_player.duration(), m_player.isSeekable() });
    }

    void publishMetadata()
    {
        const QMediaMetaData metadata = m_player.metaData();
        QImage cover = metadata.value(QMediaMetaData::CoverArtImage).value<QImage>();
        QImage thumbnail = metadata.value(QMediaMetaData::ThumbnailImage).value<QImage>();
        if (cover.isNull() && thumbnail.isNull()) {
            return;
        }
        kiriview::invokeIfSet(m_callbacks.metadataAvailable,
            kiriview::VideoThumbnailEmbeddedImages { std::move(cover), std::move(thumbnail) });
    }

    QMediaPlayer m_player;
    QVideoSink m_sink;
    kiriview::VideoThumbnailBackendCallbacks m_callbacks;
};
}

namespace kiriview {
std::unique_ptr<VideoThumbnailBackend> createDefaultVideoThumbnailBackend(QObject* parent)
{
    return std::make_unique<QtVideoThumbnailBackend>(parent);
}
}
