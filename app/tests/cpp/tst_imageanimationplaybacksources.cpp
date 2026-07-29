// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imageanimationplaybacksource.h"

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>
#include <QTest>
#include <QtGlobal>
#include <memory>
#include <utility>

namespace {
enum class PlaybackKind {
    Gif,
    Apng,
    WebP,
    Jxl,
    HeifSequence,
};

QByteArray fixtureData(const QString& fileName)
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/") + fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QByteArray twoColorGifData()
{
    return QByteArray::fromBase64(
        QByteArrayLiteral("R0lGODlhAQABAIAAAP8AAAAA/yH/C05FVFNDQVBFMi4wAwECAAAh+QQAAQAAACwAAAAAAQAB"
                          "AAACAkQBACH5BAACAAAALAAAAAABAAEAAAICTAEAOw=="));
}

kiriview::ImageAnimationPlaybackRequest playbackRequest(PlaybackKind kind, QByteArray data)
{
    switch (kind) {
    case PlaybackKind::Gif:
        return kiriview::readerAnimationPlaybackRequest(std::move(data), QByteArrayLiteral("gif"));
    case PlaybackKind::Apng:
        return kiriview::apngAnimationPlaybackRequest(std::move(data));
    case PlaybackKind::WebP:
        return kiriview::webpAnimationPlaybackRequest(std::move(data));
    case PlaybackKind::Jxl:
        return kiriview::jxlAnimationPlaybackRequest(std::move(data));
    case PlaybackKind::HeifSequence:
        return kiriview::heifSequenceAnimationPlaybackRequest(std::move(data));
    }

    Q_UNREACHABLE_RETURN({});
}

bool imagesHaveDistinctPixels(const QImage& first, const QImage& second)
{
    if (first.size() != second.size()) {
        return true;
    }
    for (int y = 0; y < first.height(); ++y) {
        for (int x = 0; x < first.width(); ++x) {
            if (first.pixelColor(x, y) != second.pixelColor(x, y)) {
                return true;
            }
        }
    }
    return false;
}
}

class TestImageAnimationPlaybackSources : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesDistinctAuthoredFrames_data();
    void preservesDistinctAuthoredFrames();
};

void TestImageAnimationPlaybackSources::preservesDistinctAuthoredFrames_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("fileName");

    QTest::newRow("gif") << static_cast<int>(PlaybackKind::Gif) << QString();
    QTest::newRow("apng") << static_cast<int>(PlaybackKind::Apng)
                          << QStringLiteral("animated-smoke.apng");
    QTest::newRow("webp") << static_cast<int>(PlaybackKind::WebP)
                          << QStringLiteral("animated-smoke.webp");
    QTest::newRow("jxl") << static_cast<int>(PlaybackKind::Jxl)
                         << QStringLiteral("animated-smoke.jxl");
    QTest::newRow("heif-sequence") << static_cast<int>(PlaybackKind::HeifSequence)
                                   << QStringLiteral("heif-sequence-alpha.heics");
}

void TestImageAnimationPlaybackSources::preservesDistinctAuthoredFrames()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);

    const QByteArray data = fileName.isEmpty() ? twoColorGifData() : fixtureData(fileName);
    QVERIFY2(!data.isEmpty(), qPrintable(fileName));
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data));
    QVERIFY(source != nullptr);

    const kiriview::ImageAnimationPlaybackOpenResult opened = source->open();
    QCOMPARE(opened.status, kiriview::ImageAnimationPlaybackOpenStatus::Success);
    QVERIFY2(!opened.firstFrame.isNull(), qPrintable(opened.errorString));
    QVERIFY(opened.sourceHasMoreFrames);

    const kiriview::ImageAnimationPlaybackReadResult second = source->readNextFrame();
    QCOMPARE(second.status, kiriview::ImageAnimationPlaybackReadStatus::Frame);
    QVERIFY2(!second.frame.image.isNull(), qPrintable(second.errorString));
    QCOMPARE(second.frame.image.size(), opened.firstFrame.size());
    QVERIFY(imagesHaveDistinctPixels(opened.firstFrame, second.frame.image));

    const kiriview::ImageAnimationPlaybackReadResult end = source->readNextFrame();
    QCOMPARE(end.status, kiriview::ImageAnimationPlaybackReadStatus::End);
}

QTEST_GUILESS_MAIN(TestImageAnimationPlaybackSources)

#include "tst_imageanimationplaybacksources.moc"
