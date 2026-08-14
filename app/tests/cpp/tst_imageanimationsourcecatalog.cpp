// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imageanimationsourcecatalog.h"

#include "decoding/animationtiming.h"
#include "decoding/apnganimationreader.h"
#include "decoding/heifsequencereader.h"
#include "decoding/imageanimationrequest.h"
#include "decoding/imagedecodeworkspace.h"

#include <QByteArray>
#include <QFile>
#include <QObject>
#include <QSize>
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

QByteArray finiteLoopGifData()
{
    return QByteArray::fromBase64(
        QByteArrayLiteral("R0lGODlhAQABAIAAAP8AAAAA/yH/C05FVFNDQVBFMi4wAwECAAAh+QQAAQAAACwAAAAAAQAB"
                          "AAACAkQBACH5BAACAAAALAAAAAABAAEAAAICTAEAOw=="));
}

kiriview::ImageAnimationPlaybackRequest playbackRequest(PlaybackKind kind, QByteArray data,
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget = {})
{
    switch (kind) {
    case PlaybackKind::Gif:
        return kiriview::readerAnimationPlaybackRequest(std::move(data), QByteArrayLiteral("gif"));
    case PlaybackKind::Apng:
        return kiriview::apngAnimationPlaybackRequest(std::move(data));
    case PlaybackKind::WebP:
        return kiriview::webpAnimationPlaybackRequest(
            std::move(data), {}, std::move(workspaceBudget));
    case PlaybackKind::Jxl:
        return kiriview::jxlAnimationPlaybackRequest(
            std::move(data), {}, std::move(workspaceBudget));
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
    void catalogWorkspaceFailureIsTyped_data();
    void catalogWorkspaceFailureIsTyped();
    void heifCatalogChargesRetainedFirstFrameSeparately();
    void heifCatalogIncludesRetainedInputInOperationAdmission();
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
    QVERIFY2(result.has_value(),
        qPrintable(result.has_value() ? QString() : result.error().errorString));
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
    QVERIFY2(result.has_value(),
        qPrintable(result.has_value() ? QString() : result.error().errorString));
    QVERIFY(result->isValid());
    QCOMPARE(result->logicalSize, QSize(2, 1));
    QCOMPARE(result->repeatCount, -1);
    QCOMPARE(result->frameDurations.size(), 2);
    for (const int duration : result->frameDurations) {
        QVERIFY(duration > 0);
        QCOMPARE(duration, kiriview::normalizedAnimationFrameDelay(duration));
    }
}

void TestImageAnimationSourceCatalog::catalogWorkspaceFailureIsTyped_data()
{
    QTest::addColumn<int>("kind");
    QTest::addColumn<QString>("fileName");

    QTest::newRow("webp") << static_cast<int>(PlaybackKind::WebP)
                          << QStringLiteral("animated-smoke.webp");
    QTest::newRow("jxl") << static_cast<int>(PlaybackKind::Jxl)
                         << QStringLiteral("animated-smoke.jxl");
}

void TestImageAnimationSourceCatalog::catalogWorkspaceFailureIsTyped()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);

    const QByteArray data = fixtureData(fileName);
    QVERIFY(!data.isEmpty());
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(1, 1);
    const kiriview::ImageAnimationSourceCatalogResult catalog
        = kiriview::readImageAnimationSourceCatalog(
            playbackRequest(static_cast<PlaybackKind>(kind), data, budget));

    QVERIFY(!catalog.has_value());
    QCOMPARE(catalog.error().errorString, kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
    QCOMPARE(catalog.error().cause,
        kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageAnimationSourceCatalog::heifCatalogChargesRetainedFirstFrameSeparately()
{
    const QByteArray data = fixtureData(QStringLiteral("heif-sequence-alpha.heics"));
    QVERIFY(!data.isEmpty());
    constexpr qsizetype generousByteCount = qsizetype { 256 } * 1024 * 1024;
    auto measurementBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, generousByteCount);
    kiriview::HeifSequenceReader measuredReader(measurementBudget);
    const kiriview::HeifSequenceOpenResult measuredOpen = measuredReader.open(data);
    QCOMPARE(measuredOpen.status, kiriview::HeifSequenceOpenStatus::Success);
    kiriview::AnimationFrameReadResult measuredFirst = measuredReader.readNextFrame();
    QVERIFY(measuredFirst && measuredFirst->has_value());
    const qsizetype firstFrameReservationByteCount = measurementBudget->reservedByteCount();
    QVERIFY(firstFrameReservationByteCount > (**measuredFirst).image.sizeInBytes());
    measuredFirst = std::optional<kiriview::AnimationFrame>();
    measuredReader.close();
    QCOMPARE(measurementBudget->reservedByteCount(), qsizetype(0));

    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        firstFrameReservationByteCount, firstFrameReservationByteCount);
    kiriview::HeifSequenceReader reader(budget);
    const kiriview::HeifSequenceOpenResult opened = reader.open(data);
    QCOMPARE(opened.status, kiriview::HeifSequenceOpenStatus::Success);
    kiriview::AnimationFrameReadResult firstFrame = reader.readNextFrame();
    QVERIFY(firstFrame && firstFrame->has_value());
    QCOMPARE(budget->reservedByteCount(), firstFrameReservationByteCount);

    const kiriview::ImageAnimationSourceCatalogResult catalog
        = kiriview::readHeifSequenceAnimationSourceCatalog(
            reader, **firstFrame, opened.repeatCount);
    QVERIFY(!catalog.has_value());
    QCOMPARE(catalog.error().errorString, kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
    QCOMPARE(catalog.error().cause,
        kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded);
    QVERIFY(reader.lastReadResourceLimitExceeded());
}

void TestImageAnimationSourceCatalog::heifCatalogIncludesRetainedInputInOperationAdmission()
{
    const QByteArray data = fixtureData(QStringLiteral("heif-sequence-alpha.heics"));
    QVERIFY(!data.isEmpty());
    constexpr qsizetype perOperationByteLimit = qsizetype { 256 } * 1024 * 1024;
    constexpr qsizetype retainedInputByteCount = perOperationByteLimit - 1;
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        perOperationByteLimit * 2, perOperationByteLimit);
    kiriview::ImageDecodeWorkspaceLease retainedInput
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*budget);
    QVERIFY(
        kiriview::ImageDecodeWorkspaceDetail::tryReserve(retainedInput, retainedInputByteCount));
    kiriview::ImageAnimationPlaybackRequest request
        = kiriview::heifSequenceAnimationPlaybackRequest(
            data, {}, budget, retainedInput.sharedHold());
    retainedInput = {};

    const kiriview::ImageAnimationSourceCatalogResult catalog
        = kiriview::readImageAnimationSourceCatalog(request);

    QVERIFY(!catalog.has_value());
    QCOMPARE(catalog.error().cause,
        kiriview::ImageAnimationSourceCatalogFailureCause::ResourceLimitExceeded);
    QCOMPARE(catalog.error().errorString, kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
    QCOMPARE(budget->reservedByteCount(), retainedInputByteCount);

    request = {};
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestImageAnimationSourceCatalog)

#include "tst_imageanimationsourcecatalog.moc"
