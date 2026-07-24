// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "qtmultimediathumbnailbackend_p.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

#include <chrono>
#include <memory>
#include <optional>
#include <utility>

namespace {

using kiriview::detail::QtVideoThumbnailBackendResources;
using kiriview::detail::VideoThumbnailBackendError;
using kiriview::detail::VideoThumbnailBackendFrame;
using kiriview::detail::VideoThumbnailBackendMediaFacts;
using kiriview::detail::VideoThumbnailBackendMediaStatus;

class QtMultimediaThumbnailBackendTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mediaStatusesAreProjected_data();
    void mediaStatusesAreProjected();
    void errorSignalsAreProjected_data();
    void errorSignalsAreProjected();
    void positionSignalsAreProjected();
    void metadataImagesAreProjected();
    void framesAndTeardownUseQtResources();
    void deadlineUsesMonotonicExpiryAndCanBeStopped();
};

void QtMultimediaThumbnailBackendTest::mediaStatusesAreProjected_data()
{
    QTest::addColumn<QMediaPlayer::MediaStatus>("qtStatus");
    QTest::addColumn<VideoThumbnailBackendMediaStatus>("backendStatus");

    QTest::newRow("no-media") << QMediaPlayer::NoMedia << VideoThumbnailBackendMediaStatus::Pending;
    QTest::newRow("loading") << QMediaPlayer::LoadingMedia
                             << VideoThumbnailBackendMediaStatus::Pending;
    QTest::newRow("loaded") << QMediaPlayer::LoadedMedia << VideoThumbnailBackendMediaStatus::Ready;
    QTest::newRow("stalled") << QMediaPlayer::StalledMedia
                             << VideoThumbnailBackendMediaStatus::Pending;
    QTest::newRow("buffering") << QMediaPlayer::BufferingMedia
                               << VideoThumbnailBackendMediaStatus::Pending;
    QTest::newRow("buffered") << QMediaPlayer::BufferedMedia
                              << VideoThumbnailBackendMediaStatus::Ready;
    QTest::newRow("end") << QMediaPlayer::EndOfMedia
                         << VideoThumbnailBackendMediaStatus::EndOfMedia;
    QTest::newRow("invalid") << QMediaPlayer::InvalidMedia
                             << VideoThumbnailBackendMediaStatus::Invalid;
}

void QtMultimediaThumbnailBackendTest::mediaStatusesAreProjected()
{
    QFETCH(QMediaPlayer::MediaStatus, qtStatus);
    QFETCH(VideoThumbnailBackendMediaStatus, backendStatus);

    QCOMPARE(kiriview::detail::projectQtVideoThumbnailMediaStatus(qtStatus), backendStatus);
}

void QtMultimediaThumbnailBackendTest::errorSignalsAreProjected_data()
{
    QTest::addColumn<QMediaPlayer::Error>("qtError");
    QTest::addColumn<VideoThumbnailBackendError>("backendError");

    QTest::newRow("resource") << QMediaPlayer::ResourceError
                              << VideoThumbnailBackendError::Resource;
    QTest::newRow("format") << QMediaPlayer::FormatError << VideoThumbnailBackendError::Format;
    QTest::newRow("network") << QMediaPlayer::NetworkError << VideoThumbnailBackendError::Network;
    QTest::newRow("access-denied")
        << QMediaPlayer::AccessDeniedError << VideoThumbnailBackendError::AccessDenied;
}

void QtMultimediaThumbnailBackendTest::errorSignalsAreProjected()
{
    QFETCH(QMediaPlayer::Error, qtError);
    QFETCH(VideoThumbnailBackendError, backendError);

    auto player = std::make_unique<QMediaPlayer>();
    auto sink = std::make_unique<QVideoSink>();
    auto* playerProbe = player.get();
    auto backend = kiriview::detail::createQtVideoThumbnailBackend(
        QtVideoThumbnailBackendResources { std::move(player), std::move(sink) });

    std::optional<VideoThumbnailBackendError> observedError;
    QString observedDiagnostic;
    backend->setCallbacks({
        .errorOccurred =
            [&](VideoThumbnailBackendError error, QString diagnostic) {
                observedError = error;
                observedDiagnostic = std::move(diagnostic);
            },
    });

    Q_EMIT playerProbe->errorOccurred(qtError, QStringLiteral("backend diagnostic"));

    QCOMPARE(observedError, backendError);
    QCOMPARE(observedDiagnostic, QStringLiteral("backend diagnostic"));

    observedError.reset();
    Q_EMIT playerProbe->errorOccurred(QMediaPlayer::NoError, QStringLiteral("ignored"));
    QVERIFY(!observedError.has_value());
}

void QtMultimediaThumbnailBackendTest::positionSignalsAreProjected()
{
    auto player = std::make_unique<QMediaPlayer>();
    auto sink = std::make_unique<QVideoSink>();
    auto* playerProbe = player.get();
    auto backend = kiriview::detail::createQtVideoThumbnailBackend(
        QtVideoThumbnailBackendResources { std::move(player), std::move(sink) });

    std::optional<qint64> observedPosition;
    backend->setCallbacks({
        .positionChanged = [&](qint64 positionMsec) { observedPosition = positionMsec; },
    });

    Q_EMIT playerProbe->positionChanged(3210);

    QCOMPARE(observedPosition, 3210);
}

void QtMultimediaThumbnailBackendTest::metadataImagesAreProjected()
{
    QImage cover(4, 2, QImage::Format_RGBA8888);
    cover.fill(Qt::red);
    QImage thumbnail(2, 1, QImage::Format_RGBA8888);
    thumbnail.fill(Qt::blue);

    QMediaMetaData metadata;
    metadata.insert(QMediaMetaData::CoverArtImage, cover);
    metadata.insert(QMediaMetaData::ThumbnailImage, thumbnail);

    const auto images = kiriview::detail::projectQtVideoThumbnailMetadata(metadata);
    QCOMPARE(images.coverArt, cover);
    QCOMPARE(images.thumbnail, thumbnail);

    const auto emptyImages = kiriview::detail::projectQtVideoThumbnailMetadata(QMediaMetaData {});
    QVERIFY(emptyImages.coverArt.isNull());
    QVERIFY(emptyImages.thumbnail.isNull());
}

void QtMultimediaThumbnailBackendTest::framesAndTeardownUseQtResources()
{
    auto player = std::make_unique<QMediaPlayer>();
    auto sink = std::make_unique<QVideoSink>();
    auto* playerProbe = player.get();
    auto* sinkProbe = sink.get();
    auto backend = kiriview::detail::createQtVideoThumbnailBackend(
        QtVideoThumbnailBackendResources { std::move(player), std::move(sink) });

    std::optional<VideoThumbnailBackendFrame> observedFrame;
    backend->setCallbacks({
        .frameAvailable
        = [&](VideoThumbnailBackendFrame frame) { observedFrame = std::move(frame); },
    });

    QImage sourceImage(6, 4, QImage::Format_RGBA8888);
    sourceImage.fill(Qt::green);
    sinkProbe->setVideoFrame(QVideoFrame(sourceImage));

    QVERIFY(observedFrame.has_value());
    QCOMPARE(observedFrame->pixelSize, sourceImage.size());
    QVERIFY(observedFrame->materialize);
    const QImage materialized = observedFrame->materialize();
    QCOMPARE(materialized.size(), sourceImage.size());
    QCOMPARE(materialized.pixelColor(0, 0), sourceImage.pixelColor(0, 0));

    observedFrame.reset();
    sinkProbe->setVideoFrame({});
    QVERIFY(observedFrame.has_value());
    QVERIFY(observedFrame->pixelSize.isEmpty());
    QVERIFY(!observedFrame->materialize);

    const QUrl sourceUrl(QStringLiteral("file:///nonexistent-kiriview-video.mp4"));
    backend->setSource(sourceUrl);
    QCOMPARE(playerProbe->source(), sourceUrl);
    sinkProbe->setVideoFrame(QVideoFrame(sourceImage));
    QVERIFY(sinkProbe->videoFrame().isValid());

    backend->stop();

    QVERIFY(playerProbe->source().isEmpty());
    QVERIFY(!sinkProbe->videoFrame().isValid());
}

void QtMultimediaThumbnailBackendTest::deadlineUsesMonotonicExpiryAndCanBeStopped()
{
    using namespace std::chrono_literals;

    auto deadline = kiriview::detail::createQtVideoThumbnailDeadline();
    QElapsedTimer elapsed;
    int expiryCount = 0;

    elapsed.start();
    deadline->start(20ms, [&expiryCount]() { ++expiryCount; });
    QTRY_COMPARE_WITH_TIMEOUT(expiryCount, 1, 1000);
    QVERIFY(elapsed.nsecsElapsed()
        >= std::chrono::duration_cast<std::chrono::nanoseconds>(20ms).count());

    deadline->start(0ms, [&expiryCount]() { ++expiryCount; });
    deadline->stop();
    QCoreApplication::processEvents();
    QCOMPARE(expiryCount, 1);
}

} // namespace

QTEST_GUILESS_MAIN(QtMultimediaThumbnailBackendTest)

#include "tst_qtmultimediathumbnailbackend.moc"
