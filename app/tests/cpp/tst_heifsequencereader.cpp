// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/heifsequencereader.h"

#include "image_test_support.h"
#include "localization/imageerrortext.h"

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QObject>
#include <QSize>
#include <QTest>
#include <optional>

class TestHeifSequenceReader : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void rejectsNonHeifData();
    void readsFramesFromStreamingSequence();
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
    QCOMPARE(frame.error(),
        kiriview::imageErrorText(kiriview::ImageErrorTextId::HeifSequenceTrackMissing));
}

QTEST_GUILESS_MAIN(TestHeifSequenceReader)

#include "tst_heifsequencereader.moc"
