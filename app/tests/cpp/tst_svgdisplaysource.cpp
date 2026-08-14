// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "rendering/svgdisplaysource.h"

#include "decoding/imagedecodeworkspace.h"

#include <QColor>
#include <QObject>
#include <QTest>
#include <Qt>
#include <memory>
#include <optional>

namespace {
QByteArray clippedSvgData()
{
    return QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"12\" height=\"8\">"
                             "<rect width=\"12\" height=\"8\" fill=\"white\"/>"
                             "<clipPath id=\"clip\">"
                             "<rect x=\"2\" y=\"1\" width=\"4\" height=\"4\"/>"
                             "</clipPath>"
                             "<g clip-path=\"url(#clip)\">"
                             "<rect width=\"12\" height=\"8\" fill=\"red\"/>"
                             "</g>"
                             "</svg>");
}
}

class TestSvgDisplaySource : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void sourceRendersIntrinsicPreviewAndBlockingDisplay();
    void sourceOpenRequiresParserWorkspaceAdmission();
    void sourceRendersWholeSurfaceDisplayBucket();
    void sourceRendersUpscaledFirstDisplayPreview();
    void sourceSkipsFirstDisplayPreviewWithoutValidViewport();
    void initialPreflightAccountsOnlyForSelectedDisplayPath();
    void sourceReportsFirstDisplayRenderFailure();
    void sourceReportsWholeSurfaceRenderFailure();
    void workerLimitFailureIsTypedAndDoesNotPoisonLaterRendering();
    void sourceAppliesClipPathToBlockingAndBucketDisplay();
    void sourceRendersOversampledDisplayBucket();
};

void TestSvgDisplaySource::sourceOpenRequiresParserWorkspaceAdmission()
{
    const QByteArray data = QByteArrayLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\"/>");
    const std::optional<qsizetype> parserByteCost
        = kiriview::svgParserWorkspaceByteCost(data.size());
    QVERIFY(parserByteCost.has_value());
    QVERIFY(*parserByteCost > 1);

    auto insufficientBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        *parserByteCost - 1, *parserByteCost - 1);
    QString errorString;
    bool resourceExhausted = false;
    const std::shared_ptr<kiriview::SvgDisplaySource> rejected = kiriview::SvgDisplaySource::open(
        data, &errorString, insufficientBudget, &resourceExhausted);
    QVERIFY(rejected == nullptr);
    QVERIFY(resourceExhausted);
    QCOMPARE(insufficientBudget->reservedByteCount(), qsizetype(0));

    auto exactBudget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(*parserByteCost, *parserByteCost);
    resourceExhausted = false;
    const std::shared_ptr<kiriview::SvgDisplaySource> admitted
        = kiriview::SvgDisplaySource::open(data, &errorString, exactBudget, &resourceExhausted);
    QVERIFY2(admitted != nullptr, qPrintable(errorString));
    QVERIFY(!resourceExhausted);
    QCOMPARE(exactBudget->reservedByteCount(), qsizetype(0));
}

void TestSvgDisplaySource::sourceRendersIntrinsicPreviewAndBlockingDisplay()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");

    QString errorString;
    std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(data, &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));
    QCOMPARE(source->imageSize(), QSize(80, 40));

    const kiriview::FirstDisplayImageDecodeResult firstDisplay
        = source->decodeFirstDisplayImage(
                    kiriview::ImageFirstDisplayDecodeContext { QSize(20, 20) })
              .firstDisplay;
    QCOMPARE(firstDisplay.status, kiriview::FirstDisplayImageDecodeStatus::Ready);
    QCOMPARE(firstDisplay.image.size(), QSize(20, 10));

    const kiriview::StaticImageDisplayDecodeResult preview = source->decodeBlockingDisplayImage(20);
    QVERIFY2(!preview.image.isNull(), qPrintable(preview.diagnostics.userMessage));
    QCOMPARE(preview.image.size(), QSize(20, 10));

    const kiriview::StaticImageDisplayDecodeResult bucket
        = source->decodeRasterDisplayImage(QSize(80, 40));
    QVERIFY2(!bucket.image.isNull(), qPrintable(bucket.diagnostics.userMessage));
    QCOMPARE(bucket.image.size(), QSize(80, 40));
    QCOMPARE(bucket.image.pixelColor(10, 10), QColor(Qt::red));
}

void TestSvgDisplaySource::sourceRendersWholeSurfaceDisplayBucket()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");

    QString errorString;
    std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(data, &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));
    QVERIFY(source->supportsRasterDisplayRefinement());
    const std::optional<qsizetype> parserByteCost
        = kiriview::svgParserWorkspaceByteCost(data.size());
    QVERIFY(parserByteCost.has_value());
    QCOMPARE(source->rasterDisplayRefinementPeakByteCost(QSize(120, 60)),
        std::optional<qsizetype>(*parserByteCost + 2 * 120 * 60 * 4));

    const kiriview::StaticImageDisplayDecodeResult bucket
        = source->decodeRasterDisplayImage(QSize(120, 60));

    QVERIFY2(!bucket.image.isNull(), qPrintable(bucket.diagnostics.userMessage));
    QCOMPARE(bucket.image.size(), QSize(120, 60));
    QCOMPARE(bucket.image.pixelColor(10, 10), QColor(Qt::red));
}

void TestSvgDisplaySource::sourceRendersUpscaledFirstDisplayPreview()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");

    QString errorString;
    std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(data, &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::FirstDisplayImageDecodeResult firstDisplay
        = source
              ->decodeFirstDisplayImage(
                  kiriview::ImageFirstDisplayDecodeContext { QSize(200, 200) })
              .firstDisplay;

    QCOMPARE(firstDisplay.status, kiriview::FirstDisplayImageDecodeStatus::Ready);
    QCOMPARE(firstDisplay.image.size(), QSize(200, 100));
}

void TestSvgDisplaySource::sourceSkipsFirstDisplayPreviewWithoutValidViewport()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");

    QString errorString;
    std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(data, &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::FirstDisplayImageDecodeResult firstDisplay
        = source->decodeFirstDisplayImage(kiriview::ImageFirstDisplayDecodeContext {}).firstDisplay;

    QCOMPARE(firstDisplay.status, kiriview::FirstDisplayImageDecodeStatus::NotImplemented);
    QVERIFY(firstDisplay.image.isNull());
}

void TestSvgDisplaySource::initialPreflightAccountsOnlyForSelectedDisplayPath()
{
    kiriview::SvgDisplaySource source({}, QSize(4000, 2000));
    const auto firstDisplayPeak = source.rasterDisplayRefinementPeakByteCost(QSize(400, 200));
    const auto blockingPeak = source.rasterDisplayRefinementPeakByteCost(QSize(2048, 1024));
    QVERIFY(firstDisplayPeak.has_value());
    QVERIFY(blockingPeak.has_value());
    QVERIFY(*firstDisplayPeak < *blockingPeak);

    const auto selectedPeak = source.initialDisplayDecodePeakByteCost(
        kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) }, 2048);
    const auto fallbackPeak = source.initialDisplayDecodePeakByteCost({}, 2048);
    QVERIFY(selectedPeak.has_value());
    QVERIFY(fallbackPeak.has_value());
    QVERIFY(*selectedPeak >= *firstDisplayPeak);
    QVERIFY(*selectedPeak < *blockingPeak);
    QVERIFY(*fallbackPeak >= *blockingPeak);
}

void TestSvgDisplaySource::sourceReportsFirstDisplayRenderFailure()
{
    kiriview::SvgDisplaySource source(QByteArrayLiteral("not svg"), QSize(80, 40));

    const kiriview::StaticImageFirstDisplayDecodeResult decoded = source.decodeFirstDisplayImage(
        kiriview::ImageFirstDisplayDecodeContext { QSize(20, 20) });
    const kiriview::FirstDisplayImageDecodeResult& firstDisplay = decoded.firstDisplay;

    QCOMPARE(firstDisplay.status, kiriview::FirstDisplayImageDecodeStatus::Error);
    QVERIFY(firstDisplay.image.isNull());
    QVERIFY(!decoded.diagnostics.userMessage.isEmpty());
}

void TestSvgDisplaySource::sourceReportsWholeSurfaceRenderFailure()
{
    kiriview::SvgDisplaySource source(QByteArrayLiteral("not svg"), QSize(80, 40));

    const kiriview::StaticImageDisplayDecodeResult bucket
        = source.decodeRasterDisplayImage(QSize(120, 60));

    QVERIFY(source.supportsRasterDisplayRefinement());
    QVERIFY(bucket.image.isNull());
    QVERIFY(!bucket.diagnostics.userMessage.isEmpty());
}

void TestSvgDisplaySource::workerLimitFailureIsTypedAndDoesNotPoisonLaterRendering()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");
    QString errorString;
    const std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(data, &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::StaticImageDisplayDecodeResult rejected
        = source->decodeRasterDisplayImage(QSize(32'768, 32'768));
    QVERIFY(rejected.image.isNull());
    QCOMPARE(
        rejected.failureCause, kiriview::StaticImageDisplayDecodeFailureCause::ResourceExhausted);

    const kiriview::StaticImageDisplayDecodeResult recovered
        = source->decodeRasterDisplayImage(QSize(80, 40));
    QVERIFY2(!recovered.image.isNull(), qPrintable(recovered.diagnostics.userMessage));
    QCOMPARE(recovered.image.size(), QSize(80, 40));
}

void TestSvgDisplaySource::sourceAppliesClipPathToBlockingAndBucketDisplay()
{
    QString errorString;
    std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(clippedSvgData(), &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));
    QCOMPARE(source->imageSize(), QSize(12, 8));

    const kiriview::StaticImageDisplayDecodeResult preview = source->decodeBlockingDisplayImage(12);
    QVERIFY2(!preview.image.isNull(), qPrintable(preview.diagnostics.userMessage));
    QCOMPARE(preview.image.pixelColor(3, 2), QColor(Qt::red));
    QCOMPARE(preview.image.pixelColor(8, 2), QColor(Qt::white));

    const kiriview::StaticImageDisplayDecodeResult bucket
        = source->decodeRasterDisplayImage(QSize(12, 8));
    QVERIFY2(!bucket.image.isNull(), qPrintable(bucket.diagnostics.userMessage));
    QCOMPARE(bucket.image.pixelColor(3, 2), QColor(Qt::red));
    QCOMPARE(bucket.image.pixelColor(8, 2), QColor(Qt::white));
}

void TestSvgDisplaySource::sourceRendersOversampledDisplayBucket()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");

    QString errorString;
    std::shared_ptr<kiriview::SvgDisplaySource> source
        = kiriview::SvgDisplaySource::open(data, &errorString);
    QVERIFY2(source != nullptr, qPrintable(errorString));

    const kiriview::StaticImageDisplayDecodeResult bucket
        = source->decodeRasterDisplayImage(QSize(120, 60));
    QVERIFY2(!bucket.image.isNull(), qPrintable(bucket.diagnostics.userMessage));
    QCOMPARE(bucket.image.size(), QSize(120, 60));
    QCOMPARE(bucket.image.pixelColor(10, 10), QColor(Qt::red));
}

QTEST_GUILESS_MAIN(TestSvgDisplaySource)

#include "tst_svgdisplaysource.moc"
