// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imageanimationsourcecatalog.h"

#include "decoding/animationtiming.h"
#include "decoding/apnganimationreader.h"
#include "decoding/imageanimationrequest.h"

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QSize>
#include <QString>
#include <QTest>
#include <QtGlobal>
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

QByteArray finiteLoopGifData()
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

quint32 readBigEndian32(const QByteArray& data, qsizetype offset)
{
    return (static_cast<quint32>(static_cast<unsigned char>(data[offset])) << 24)
        | (static_cast<quint32>(static_cast<unsigned char>(data[offset + 1])) << 16)
        | (static_cast<quint32>(static_cast<unsigned char>(data[offset + 2])) << 8)
        | static_cast<quint32>(static_cast<unsigned char>(data[offset + 3]));
}

void writeBigEndian32(QByteArray* data, qsizetype offset, quint32 value)
{
    (*data)[offset] = static_cast<char>((value >> 24) & 0xff);
    (*data)[offset + 1] = static_cast<char>((value >> 16) & 0xff);
    (*data)[offset + 2] = static_cast<char>((value >> 8) & 0xff);
    (*data)[offset + 3] = static_cast<char>(value & 0xff);
}

quint32 crc32(const QByteArray& data)
{
    quint32 crc = 0xffffffffU;
    for (const unsigned char byte : data) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return crc ^ 0xffffffffU;
}

QByteArray apngWithUndecodableLaterRaster()
{
    QByteArray data = fixtureData(QStringLiteral("animated-smoke.apng"));
    qsizetype offset = 8;
    while (offset + 12 <= data.size()) {
        const quint32 payloadSize = readBigEndian32(data, offset);
        if (payloadSize > static_cast<quint32>(data.size() - offset - 12)) {
            return {};
        }

        const qsizetype typeOffset = offset + 4;
        const qsizetype payloadOffset = typeOffset + 4;
        const qsizetype crcOffset = payloadOffset + static_cast<qsizetype>(payloadSize);
        if (data.mid(typeOffset, 4) == QByteArrayLiteral("fdAT") && payloadSize > 4) {
            for (qsizetype index = payloadOffset + 4; index < crcOffset; ++index) {
                data[index] = '\0';
            }
            writeBigEndian32(&data, crcOffset, crc32(data.mid(typeOffset, 4 + payloadSize)));
            return data;
        }
        offset = crcOffset + 4;
    }
    return {};
}
}

class TestImageAnimationSourceCatalog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void realAnimationSourcesExposeNormalizedCatalogs_data();
    void realAnimationSourcesExposeNormalizedCatalogs();
    void apngCatalogDoesNotRequireLaterRasterDecode();
};

void TestImageAnimationSourceCatalog::realAnimationSourcesExposeNormalizedCatalogs_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QSize>("logicalSize");
    QTest::addColumn<int>("repeatCount");

    QTest::newRow("gif-finite-loop")
        << static_cast<int>(PlaybackKind::Gif) << QString() << QSize(1, 1) << 2;
    QTest::newRow("apng") << static_cast<int>(PlaybackKind::Apng)
                          << QStringLiteral("animated-smoke.apng") << QSize(2, 1) << -1;
    QTest::newRow("webp") << static_cast<int>(PlaybackKind::WebP)
                          << QStringLiteral("animated-smoke.webp") << QSize(2, 1) << -1;
    QTest::newRow("jxl") << static_cast<int>(PlaybackKind::Jxl)
                         << QStringLiteral("animated-smoke.jxl") << QSize(2, 1) << -1;
    QTest::newRow("heif-sequence")
        << static_cast<int>(PlaybackKind::HeifSequence)
        << QStringLiteral("heif-sequence-alpha.heics") << QSize(64, 64) << -1;
}

void TestImageAnimationSourceCatalog::realAnimationSourcesExposeNormalizedCatalogs()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);
    QFETCH(QSize, logicalSize);
    QFETCH(int, repeatCount);

    const QByteArray data = fileName.isEmpty() ? finiteLoopGifData() : fixtureData(fileName);
    QVERIFY2(!data.isEmpty(), qPrintable(fileName));

    kiriview::ImageAnimationSourceCatalogResult result = kiriview::readImageAnimationSourceCatalog(
        playbackRequest(static_cast<PlaybackKind>(kind), data));
    QVERIFY2(result.has_value(), qPrintable(result.has_value() ? QString() : result.error()));
    QVERIFY(result->isValid());
    QCOMPARE(result->logicalSize, logicalSize);
    QCOMPARE(result->repeatCount, repeatCount);
    QVERIFY(result->frameDurations.size() >= 2);
    for (const int duration : result->frameDurations) {
        QVERIFY(duration > 0);
        QCOMPARE(duration, kiriview::normalizedAnimationFrameDelay(duration));
    }
}

void TestImageAnimationSourceCatalog::apngCatalogDoesNotRequireLaterRasterDecode()
{
    const QByteArray data = apngWithUndecodableLaterRaster();
    QVERIFY(!data.isEmpty());

    kiriview::ApngAnimationReader reader;
    const kiriview::ApngOpenResult openResult = reader.open(data);
    QCOMPARE(openResult.status, kiriview::ApngOpenStatus::Success);
    const kiriview::AnimationFrameReadResult laterFrame = reader.readNextFrame();
    QVERIFY2(!laterFrame.has_value(), "The test input must have an undecodable later frame");

    kiriview::ImageAnimationSourceCatalogResult result
        = kiriview::readImageAnimationSourceCatalog(kiriview::apngAnimationPlaybackRequest(data));
    QVERIFY2(result.has_value(), qPrintable(result.has_value() ? QString() : result.error()));
    QVERIFY(result->isValid());
    QCOMPARE(result->logicalSize, QSize(2, 1));
    QCOMPARE(result->repeatCount, -1);
    QCOMPARE(result->frameDurations.size(), 2);
    for (const int duration : result->frameDurations) {
        QVERIFY(duration > 0);
        QCOMPARE(duration, kiriview::normalizedAnimationFrameDelay(duration));
    }
}

QTEST_GUILESS_MAIN(TestImageAnimationSourceCatalog)

#include "tst_imageanimationsourcecatalog.moc"
