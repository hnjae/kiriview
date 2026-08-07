// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imageanimationplaybacksource.h"

#include "decoding/imagedecodeworkspace.h"
#include "decoding/webpanimationreader.h"

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

kiriview::ImageAnimationPlaybackRequest playbackRequest(PlaybackKind kind, QByteArray data,
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> workspaceBudget = {})
{
    switch (kind) {
    case PlaybackKind::Gif:
        return kiriview::readerAnimationPlaybackRequest(
            std::move(data), QByteArrayLiteral("gif"), {}, std::move(workspaceBudget));
    case PlaybackKind::Apng:
        return kiriview::apngAnimationPlaybackRequest(
            std::move(data), {}, std::move(workspaceBudget));
    case PlaybackKind::WebP:
        return kiriview::webpAnimationPlaybackRequest(
            std::move(data), {}, std::move(workspaceBudget));
    case PlaybackKind::Jxl:
        return kiriview::jxlAnimationPlaybackRequest(
            std::move(data), {}, std::move(workspaceBudget));
    case PlaybackKind::HeifSequence:
        return kiriview::heifSequenceAnimationPlaybackRequest(
            std::move(data), {}, std::move(workspaceBudget));
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
    void playbackUsesWorkspaceBudget_data();
    void playbackUsesWorkspaceBudget();
    void laterFrameRetainsWorkspaceUntilResultRelease_data();
    void laterFrameRetainsWorkspaceUntilResultRelease();
    void retainedFirstFrameCanRejectLaterFrame_data();
    void retainedFirstFrameCanRejectLaterFrame();
    void perOperationLimitIncludesReaderWorkspace_data();
    void perOperationLimitIncludesReaderWorkspace();
    void webpWorkspaceModelRejectsUnauditedLibraryVersions_data();
    void webpWorkspaceModelRejectsUnauditedLibraryVersions();
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

void TestImageAnimationPlaybackSources::playbackUsesWorkspaceBudget_data()
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

void TestImageAnimationPlaybackSources::playbackUsesWorkspaceBudget()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);

    const QByteArray data = fileName.isEmpty() ? twoColorGifData() : fixtureData(fileName);
    QVERIFY(!data.isEmpty());
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(1, 1);
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data, budget));
    QVERIFY(source != nullptr);

    const kiriview::ImageAnimationPlaybackOpenResult opened = source->open();

    QCOMPARE(opened.status, kiriview::ImageAnimationPlaybackOpenStatus::ResourceLimitExceeded);
    QVERIFY(opened.firstFrame.isNull());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageAnimationPlaybackSources::laterFrameRetainsWorkspaceUntilResultRelease_data()
{
    playbackUsesWorkspaceBudget_data();
}

void TestImageAnimationPlaybackSources::laterFrameRetainsWorkspaceUntilResultRelease()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);

    const QByteArray data = fileName.isEmpty() ? twoColorGifData() : fixtureData(fileName);
    QVERIFY(!data.isEmpty());
    constexpr qsizetype budgetByteCount = qsizetype { 256 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(budgetByteCount, budgetByteCount);
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data, budget));
    QVERIFY(source != nullptr);

    kiriview::ImageAnimationPlaybackOpenResult opened = source->open();
    QCOMPARE(opened.status, kiriview::ImageAnimationPlaybackOpenStatus::Success);
    QVERIFY2(!opened.firstFrame.isNull(), qPrintable(opened.errorString));
    QVERIFY(opened.sourceHasMoreFrames);
    const qsizetype outputByteCount = opened.firstFrame.sizeInBytes();
    QVERIFY(outputByteCount > 0);
    const qsizetype openReservationByteCount = budget->reservedByteCount();
    QVERIFY(openReservationByteCount > outputByteCount);

    kiriview::ImageAnimationPlaybackReadResult laterFrame = source->readNextFrame();
    QCOMPARE(laterFrame.status, kiriview::ImageAnimationPlaybackReadStatus::Frame);
    QVERIFY2(!laterFrame.frame.image.isNull(), qPrintable(laterFrame.errorString));
    QVERIFY(laterFrame.frame.workspaceHold.isManaged());
    QCOMPARE(budget->reservedByteCount(), openReservationByteCount + outputByteCount);

    source.reset();
    QCOMPARE(budget->reservedByteCount(), 2 * outputByteCount);

    laterFrame.frame = {};
    QCOMPARE(budget->reservedByteCount(), outputByteCount);
    opened.firstFrame = {};
    opened.workspaceHold = {};
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageAnimationPlaybackSources::retainedFirstFrameCanRejectLaterFrame_data()
{
    playbackUsesWorkspaceBudget_data();
}

void TestImageAnimationPlaybackSources::retainedFirstFrameCanRejectLaterFrame()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);

    const QByteArray data = fileName.isEmpty() ? twoColorGifData() : fixtureData(fileName);
    QVERIFY(!data.isEmpty());
    constexpr qsizetype generousByteCount = qsizetype { 256 } * 1024 * 1024;
    auto measurementBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, generousByteCount);
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> measuredSource
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data, measurementBudget));
    kiriview::ImageAnimationPlaybackOpenResult measuredOpen = measuredSource->open();
    QCOMPARE(measuredOpen.status, kiriview::ImageAnimationPlaybackOpenStatus::Success);
    const qsizetype openReservationByteCount = measurementBudget->reservedByteCount();
    QVERIFY(openReservationByteCount > measuredOpen.firstFrame.sizeInBytes());
    measuredOpen.firstFrame = {};
    measuredOpen.workspaceHold = {};
    measuredSource.reset();
    QCOMPARE(measurementBudget->reservedByteCount(), qsizetype(0));

    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        openReservationByteCount, openReservationByteCount);
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data, budget));
    kiriview::ImageAnimationPlaybackOpenResult opened = source->open();
    QCOMPARE(opened.status, kiriview::ImageAnimationPlaybackOpenStatus::Success);
    const qsizetype outputByteCount = opened.firstFrame.sizeInBytes();
    QCOMPARE(budget->reservedByteCount(), openReservationByteCount);

    const kiriview::ImageAnimationPlaybackReadResult laterFrame = source->readNextFrame();
    QCOMPARE(laterFrame.status, kiriview::ImageAnimationPlaybackReadStatus::ResourceLimitExceeded);
    QVERIFY(laterFrame.frame.image.isNull());
    QCOMPARE(budget->reservedByteCount(), outputByteCount);
}

void TestImageAnimationPlaybackSources::perOperationLimitIncludesReaderWorkspace_data()
{
    playbackUsesWorkspaceBudget_data();
}

void TestImageAnimationPlaybackSources::perOperationLimitIncludesReaderWorkspace()
{
    QFETCH(int, kind);
    QFETCH(QString, fileName);

    const QByteArray data = fileName.isEmpty() ? twoColorGifData() : fixtureData(fileName);
    QVERIFY(!data.isEmpty());
    constexpr qsizetype generousByteCount = qsizetype { 256 } * 1024 * 1024;
    auto measurementBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, generousByteCount);
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> measuredSource
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data, measurementBudget));
    kiriview::ImageAnimationPlaybackOpenResult measuredOpen = measuredSource->open();
    QCOMPARE(measuredOpen.status, kiriview::ImageAnimationPlaybackOpenStatus::Success);
    const qsizetype operationByteCount = measurementBudget->reservedByteCount();
    QVERIFY(operationByteCount > measuredOpen.firstFrame.sizeInBytes());
    measuredOpen.firstFrame = {};
    measuredOpen.workspaceHold = {};
    measuredSource.reset();

    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, operationByteCount - 1);
    std::unique_ptr<kiriview::ImageAnimationPlaybackSource> source
        = kiriview::makeImageAnimationPlaybackSource(
            playbackRequest(static_cast<PlaybackKind>(kind), data, budget));
    const kiriview::ImageAnimationPlaybackOpenResult opened = source->open();
    QCOMPARE(opened.status, kiriview::ImageAnimationPlaybackOpenStatus::ResourceLimitExceeded);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageAnimationPlaybackSources::webpWorkspaceModelRejectsUnauditedLibraryVersions_data()
{
    QTest::addColumn<int>("decoderVersion");
    QTest::addColumn<int>("demuxVersion");

    QTest::newRow("decoder") << kiriview::auditedWebPAnimationLibraryVersion - 1
                             << kiriview::auditedWebPAnimationLibraryVersion;
    QTest::newRow("demux") << kiriview::auditedWebPAnimationLibraryVersion
                           << kiriview::auditedWebPAnimationLibraryVersion + 1;
}

void TestImageAnimationPlaybackSources::webpWorkspaceModelRejectsUnauditedLibraryVersions()
{
    QFETCH(int, decoderVersion);
    QFETCH(int, demuxVersion);

    const QByteArray data = fixtureData(QStringLiteral("animated-smoke.webp"));
    QVERIFY(!data.isEmpty());
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        qsizetype { 256 } * 1024 * 1024, qsizetype { 256 } * 1024 * 1024);
    kiriview::WebPAnimationReader reader(
        budget, kiriview::WebPAnimationLibraryVersions { decoderVersion, demuxVersion });

    const kiriview::WebPAnimationOpenResult opened = reader.open(data);

    QCOMPARE(opened.status, kiriview::WebPAnimationOpenStatus::ResourceLimitExceeded);
    QCOMPARE(opened.errorString, kiriview::imageDecodeWorkspaceResourceLimitDiagnostic());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestImageAnimationPlaybackSources)

#include "tst_imageanimationplaybacksources.moc"
