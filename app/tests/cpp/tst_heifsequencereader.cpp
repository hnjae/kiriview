// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/heifsequencereader.h"

#include "image_test_support.h"

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QObject>
#include <QSize>
#include <QTest>
#include <memory>
#include <optional>

class TestHeifSequenceReader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsNonHeifData();
    void boundedProbeDeclaresCompleteSequenceEnvelope();
    void readsFramesFromStreamingSequence();
    void retainedInputBaselineParticipatesInOpenAdmission();
    void reopensAtTheStartOfTheAuthoredCycle();
    void closeClearsTheActiveSequence();
};

namespace {
using kiriview::TestSupport::heifFtypBox;

QByteArray fixtureData()
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/heif-sequence-alpha.heics"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
}

void TestHeifSequenceReader::rejectsNonHeifData()
{
    kiriview::HeifSequenceReader reader;

    const kiriview::HeifSequenceOpenResult emptyResult = reader.open({});
    QCOMPARE(emptyResult.status, kiriview::HeifSequenceOpenStatus::NotHeif);

    const kiriview::HeifSequenceOpenResult nonHeifResult = reader.open(heifFtypBox("png ", {}));
    QCOMPARE(nonHeifResult.status, kiriview::HeifSequenceOpenStatus::NotHeif);
}

void TestHeifSequenceReader::boundedProbeDeclaresCompleteSequenceEnvelope()
{
    const QByteArray imageData = fixtureData();
    QVERIFY(!imageData.isEmpty());
    constexpr qsizetype generousByteCount = qsizetype { 256 } * 1024 * 1024;
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, generousByteCount);

    const kiriview::HeifSequenceWorkspacePlanResult planning
        = kiriview::planHeifSequenceOpen(imageData, budget);

    QCOMPARE(planning.status, kiriview::HeifSequenceOpenStatus::Success);
    QCOMPARE(planning.plan.imageSize, QSize(64, 64));
    QVERIFY(planning.plan.transientByteCount >= kiriview::heifSequenceProbeWorkspaceByteCount);
    QVERIFY(planning.plan.outputByteCount > 0);
    QVERIFY(planning.plan.transientByteCount + planning.plan.outputByteCount <= generousByteCount);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));

    kiriview::HeifSequenceReader reader(budget);
    const kiriview::HeifSequenceOpenResult opened = reader.open(imageData, planning.plan);
    QCOMPARE(opened.status, kiriview::HeifSequenceOpenStatus::Success);
    QCOMPARE(budget->reservedByteCount(), planning.plan.transientByteCount);
    const kiriview::AnimationFrameReadResult firstFrame = reader.readNextFrame();
    QVERIFY(firstFrame && firstFrame->has_value());
    QCOMPARE(budget->reservedByteCount(),
        planning.plan.transientByteCount + planning.plan.outputByteCount);
}

void TestHeifSequenceReader::readsFramesFromStreamingSequence()
{
    const QByteArray imageData = fixtureData();
    QVERIFY(!imageData.isEmpty());

    kiriview::HeifSequenceReader reader;
    const kiriview::HeifSequenceOpenResult openResult = reader.open(imageData);
    QCOMPARE(openResult.status, kiriview::HeifSequenceOpenStatus::Success);
    QCOMPARE(openResult.repeatCount, -1);

    const kiriview::AnimationFrameReadResult firstFrame = reader.readNextFrame();
    QVERIFY2(firstFrame.has_value(), firstFrame ? "missing frame" : qPrintable(firstFrame.error()));
    QVERIFY(firstFrame->has_value());
    QCOMPARE((**firstFrame).image.size(), QSize(64, 64));
    QVERIFY((**firstFrame).delay > 0);
    QVERIFY(qAlpha((**firstFrame).image.pixel(16, 32)) > 0);
    QVERIFY(qAlpha((**firstFrame).image.pixel(48, 32)) < 255);

    const kiriview::AnimationFrameReadResult secondFrame = reader.readNextFrame();
    QVERIFY2(
        secondFrame.has_value(), secondFrame ? "missing frame" : qPrintable(secondFrame.error()));
    QVERIFY(secondFrame->has_value());
    QCOMPARE((**secondFrame).image.size(), QSize(64, 64));
    QVERIFY((**secondFrame).delay > 0);
    QVERIFY(qAlpha((**secondFrame).image.pixel(16, 32)) < 255);
    QVERIFY(qAlpha((**secondFrame).image.pixel(48, 32)) > 0);

    const kiriview::AnimationFrameReadResult end = reader.readNextFrame();
    QVERIFY2(end.has_value(), end ? "unexpected frame" : qPrintable(end.error()));
    QVERIFY(!end->has_value());
}

void TestHeifSequenceReader::retainedInputBaselineParticipatesInOpenAdmission()
{
    const QByteArray imageData = fixtureData();
    QVERIFY(!imageData.isEmpty());
    constexpr qsizetype generousByteCount = qsizetype { 256 } * 1024 * 1024;
    auto measurementBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        generousByteCount, generousByteCount);
    kiriview::HeifSequenceReader measuredReader(measurementBudget);
    QCOMPARE(measuredReader.open(imageData).status, kiriview::HeifSequenceOpenStatus::Success);
    const qsizetype openByteCount = measurementBudget->reservedByteCount();
    QVERIFY(openByteCount > 0);
    measuredReader.close();

    auto limitedBudget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(openByteCount + 1, openByteCount);
    kiriview::HeifSequenceReader reader(limitedBudget, 1);

    QCOMPARE(
        reader.open(imageData).status, kiriview::HeifSequenceOpenStatus::ResourceLimitExceeded);
    QCOMPARE(limitedBudget->reservedByteCount(), qsizetype(0));
}

void TestHeifSequenceReader::reopensAtTheStartOfTheAuthoredCycle()
{
    const QByteArray imageData = fixtureData();
    QVERIFY(!imageData.isEmpty());

    kiriview::HeifSequenceReader reader;
    QCOMPARE(reader.open(imageData).status, kiriview::HeifSequenceOpenStatus::Success);
    const kiriview::AnimationFrameReadResult firstPass = reader.readNextFrame();
    QVERIFY(firstPass && firstPass->has_value());
    const QColor firstColor = (**firstPass).image.pixelColor(16, 32);

    QCOMPARE(reader.open(imageData).status, kiriview::HeifSequenceOpenStatus::Success);
    const kiriview::AnimationFrameReadResult reopened = reader.readNextFrame();
    QVERIFY(reopened && reopened->has_value());
    QCOMPARE((**reopened).image.pixelColor(16, 32), firstColor);
}

void TestHeifSequenceReader::closeClearsTheActiveSequence()
{
    const QByteArray imageData = fixtureData();
    QVERIFY(!imageData.isEmpty());

    kiriview::HeifSequenceReader reader;
    const kiriview::HeifSequenceOpenResult openResult = reader.open(imageData);
    QCOMPARE(openResult.status, kiriview::HeifSequenceOpenStatus::Success);

    reader.close();

    const kiriview::AnimationFrameReadResult frame = reader.readNextFrame();
    QVERIFY(!frame.has_value());
    QVERIFY(!frame.error().isEmpty());
}

QTEST_GUILESS_MAIN(TestHeifSequenceReader)

#include "tst_heifsequencereader.moc"
