// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/staticimagedecode.h"

#include "decoding/imagedecoderequest.h"
#include "decoding/imagedecodeworkspace.h"

#include <QImage>
#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace {
QImage testImage(const QSize& size)
{
    QImage image(size, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    return image;
}

class ResultDisplaySource final : public kiriview::StaticImageDisplaySource
{
public:
    explicit ResultDisplaySource(QSize imageSize = QSize(16, 16))
        : m_imageSize(std::move(imageSize))
    {
    }

    QSize imageSize() const override { return m_imageSize; }
    qsizetype byteCost() const override { return 1; }

    std::optional<qsizetype> initialDisplayDecodePeakByteCost(
        const kiriview::ImageFirstDisplayDecodeContext&, int) const override
    {
        return producerPeakByteCost;
    }

    kiriview::StaticImageFirstDisplayDecodeResult decodeFirstDisplayImage(
        const kiriview::ImageFirstDisplayDecodeContext&) const override
    {
        ++firstDisplayDecodeCount;
        kiriview::StaticImageFirstDisplayDecodeResult result;
        result.firstDisplay = firstDisplayResult;
        if (firstDisplayResult.status == kiriview::FirstDisplayImageDecodeStatus::Error) {
            result.diagnostics.diagnosticDetail = firstDisplayDiagnosticDetail.isEmpty()
                ? firstDisplayError
                : firstDisplayDiagnosticDetail;
        }
        return result;
    }

    kiriview::StaticImageDisplayDecodeResult decodeBlockingDisplayImage(
        int maximumLongEdge) const override
    {
        ++blockingDisplayDecodeCount;
        blockingDisplayMaximumLongEdge = maximumLongEdge;

        kiriview::StaticImageDisplayDecodeResult result;
        result.image = blockingDisplay;
        if (blockingDisplay.isNull()) {
            result.diagnostics.diagnosticDetail = blockingDisplayDiagnosticDetail.isEmpty()
                ? blockingDisplayError
                : blockingDisplayDiagnosticDetail;
        }
        return result;
    }

    kiriview::FirstDisplayImageDecodeResult firstDisplayResult;
    QImage blockingDisplay;
    QString firstDisplayError;
    QString blockingDisplayError;
    QString firstDisplayDiagnosticDetail;
    QString blockingDisplayDiagnosticDetail;
    qsizetype producerPeakByteCost = qsizetype { 64 } * 1024 * 1024;
    mutable int firstDisplayDecodeCount = 0;
    mutable int blockingDisplayDecodeCount = 0;
    mutable int blockingDisplayMaximumLongEdge = 0;

private:
    QSize m_imageSize;
};

const kiriview::StaticDecodedImage* staticDecodedImage(const kiriview::DecodedImageResult& result)
{
    return kiriview::decodedImageResultImageAs<kiriview::StaticDecodedImage>(result);
}

kiriview::ImageDecodeRequest testDecodeRequest(
    kiriview::ImageFirstDisplayDecodeContext firstDisplay = {})
{
    return kiriview::ImageDecodeRequest::fromUrl(
        7, QUrl::fromLocalFile(QStringLiteral("/tmp/stage3-source.jpg")), std::move(firstDisplay))
        .withSourceRevision(
            kiriview::ImageSourceRevision::fromData(QByteArrayView("static-image-decode-test")));
}
}

class TestStaticImageDecode : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void staticResultUsesReadyFirstDisplayImage();
    void staticResultFallsBackToBlockingPreview();
    void staticResultMarksScaledBlockingPreviewAsFirstDisplayQuality();
    void staticResultPreservesDecodeRouteAcrossSuccessAndFailure();
    void staticResultReportsFirstDisplayErrors();
    void staticResultReportsMissingReadyFirstDisplayImage();
    void staticResultReportsMissingBlockingPreview();
    void staticResultRejectsUnadmittedProducerBeforeRasterWork();
    void staticResultSharesAggregateAdmissionAcrossProducers();
    void staticResultRetainsAdmissionUntilLastPixelAliasRetires();
    void decodeOnlyPayloadStillReportsRetainedRasterCost();
};

void TestStaticImageDecode::staticResultUsesReadyFirstDisplayImage()
{
    auto source = std::make_shared<ResultDisplaySource>();
    source->firstDisplayResult = kiriview::FirstDisplayImageDecodeResult {
        kiriview::FirstDisplayImageDecodeStatus::Ready,
        testImage(QSize(8, 4)),
    };

    QString errorString;
    const kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(source,
        testDecodeRequest(kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) }),
        &errorString);

    const kiriview::StaticDecodedImage* decoded = staticDecodedImage(result);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->displayImage.refinementSource.get(), source.get());
    QCOMPARE(decoded->displayImage.image.size(), QSize(8, 4));
    QCOMPARE(decoded->displayImage.originalSize, QSize(16, 16));
    QCOMPARE(decoded->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);
    QCOMPARE(decoded->displayImage.sourceIdentity, QStringLiteral("file:///tmp/stage3-source.jpg"));
    QVERIFY(decoded->displayImage.isValid());
    QCOMPARE(source->firstDisplayDecodeCount, 1);
    QCOMPARE(source->blockingDisplayDecodeCount, 0);
    QVERIFY(errorString.isEmpty());
}

void TestStaticImageDecode::staticResultFallsBackToBlockingPreview()
{
    auto source = std::make_shared<ResultDisplaySource>(QSize(12, 9));
    source->blockingDisplay = testImage(QSize(12, 9));

    QString errorString;
    const kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(source,
        testDecodeRequest(kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) }),
        &errorString);

    const kiriview::StaticDecodedImage* decoded = staticDecodedImage(result);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->displayImage.refinementSource.get(), source.get());
    QCOMPARE(decoded->displayImage.image.size(), QSize(12, 9));
    QCOMPARE(decoded->displayImage.originalSize, QSize(12, 9));
    QCOMPARE(decoded->displayImage.quality, kiriview::DisplayImageQuality::Exact);
    QCOMPARE(decoded->displayImage.previewOrigin, kiriview::DisplayImagePreviewOrigin::None);
    QCOMPARE(source->firstDisplayDecodeCount, 1);
    QCOMPARE(source->blockingDisplayDecodeCount, 1);
    QCOMPARE(source->blockingDisplayMaximumLongEdge, kiriview::imageBlockingDisplayLongEdgeMax);
    QVERIFY(errorString.isEmpty());
}

void TestStaticImageDecode::staticResultMarksScaledBlockingPreviewAsFirstDisplayQuality()
{
    auto source = std::make_shared<ResultDisplaySource>(QSize(4000, 3000));
    source->blockingDisplay = testImage(QSize(2048, 1536));

    QString errorString;
    const kiriview::DecodedImageResult result
        = kiriview::staticDecodedImageResult(source, testDecodeRequest(), &errorString);

    const kiriview::StaticDecodedImage* decoded = staticDecodedImage(result);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->displayImage.image.size(), QSize(2048, 1536));
    QCOMPARE(decoded->displayImage.originalSize, QSize(4000, 3000));
    QCOMPARE(decoded->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);
    QVERIFY(errorString.isEmpty());
}

void TestStaticImageDecode::staticResultPreservesDecodeRouteAcrossSuccessAndFailure()
{
    auto successfulSource = std::make_shared<ResultDisplaySource>(QSize(12, 9));
    successfulSource->blockingDisplay = testImage(QSize(12, 9));
    QString errorString;
    const kiriview::DecodedImageResult successful
        = kiriview::staticDecodedImageResult(successfulSource, testDecodeRequest(), &errorString,
            {}, {}, kiriview::DecodedImageFailureRoute::Raw);

    const kiriview::StaticDecodedImage* decoded = staticDecodedImage(successful);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->displayImage.decodeRoute, kiriview::DecodedImageFailureRoute::Raw);

    auto failingSource = std::make_shared<ResultDisplaySource>();
    failingSource->blockingDisplayDiagnosticDetail
        = QStringLiteral("source-neutral blocking display diagnostic");
    const kiriview::DecodedImageResult failed = kiriview::staticDecodedImageResult(failingSource,
        testDecodeRequest(), &errorString, {}, {}, kiriview::DecodedImageFailureRoute::Svg);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(failed);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::Svg);
    QCOMPARE(
        failure->operation, kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage);
}

void TestStaticImageDecode::staticResultReportsFirstDisplayErrors()
{
    auto source = std::make_shared<ResultDisplaySource>();
    source->firstDisplayResult.status = kiriview::FirstDisplayImageDecodeStatus::Error;
    source->firstDisplayError = QStringLiteral("first display failed");
    source->firstDisplayDiagnosticDetail = QStringLiteral("first display backend detail");
    source->blockingDisplay = testImage(QSize(12, 9));

    QString errorString;
    const kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(source,
        testDecodeRequest(kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) }),
        &errorString);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->operation, kiriview::DecodedImageFailureOperation::DecodeFirstDisplayImage);
    QCOMPARE(failure->diagnosticDetail, QStringLiteral("first display backend detail"));
    QCOMPARE(errorString, failure->diagnosticDetail);
    QCOMPARE(failure->severity, kiriview::DecodedImageFailureSeverity::Error);
    QVERIFY(!failure->retryable);
    QCOMPARE(source->firstDisplayDecodeCount, 1);
    QCOMPARE(source->blockingDisplayDecodeCount, 0);
}

void TestStaticImageDecode::staticResultReportsMissingReadyFirstDisplayImage()
{
    auto source = std::make_shared<ResultDisplaySource>();
    source->firstDisplayResult.status = kiriview::FirstDisplayImageDecodeStatus::Ready;
    source->blockingDisplay = testImage(QSize(12, 9));

    QString errorString;
    const kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(source,
        testDecodeRequest(kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) }),
        &errorString);

    QVERIFY(kiriview::decodedImageResultFailure(result) != nullptr);
    QCOMPARE(source->firstDisplayDecodeCount, 1);
    QCOMPARE(source->blockingDisplayDecodeCount, 0);
}

void TestStaticImageDecode::staticResultReportsMissingBlockingPreview()
{
    auto source = std::make_shared<ResultDisplaySource>();
    source->blockingDisplayError = QStringLiteral("blocking display failed");
    source->blockingDisplayDiagnosticDetail = QStringLiteral("blocking display backend detail");

    QString errorString;
    const kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(source,
        testDecodeRequest(kiriview::ImageFirstDisplayDecodeContext { QSize(400, 300) }),
        &errorString);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(
        failure->operation, kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage);
    QCOMPARE(failure->diagnosticDetail, QStringLiteral("blocking display backend detail"));
    QCOMPARE(errorString, failure->diagnosticDetail);
    QCOMPARE(failure->severity, kiriview::DecodedImageFailureSeverity::Error);
    QVERIFY(!failure->retryable);
    QCOMPARE(source->firstDisplayDecodeCount, 1);
    QCOMPARE(source->blockingDisplayDecodeCount, 1);
}

void TestStaticImageDecode::staticResultRejectsUnadmittedProducerBeforeRasterWork()
{
    auto source = std::make_shared<ResultDisplaySource>();
    source->producerPeakByteCost = 257;
    source->blockingDisplay = testImage(QSize(4, 4));
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(256, 256);

    QString errorString;
    const kiriview::DecodedImageResult result
        = kiriview::staticDecodedImageResult(source, testDecodeRequest(), &errorString, budget);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QCOMPARE(source->firstDisplayDecodeCount, 0);
    QCOMPARE(source->blockingDisplayDecodeCount, 0);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestStaticImageDecode::staticResultSharesAggregateAdmissionAcrossProducers()
{
    constexpr std::size_t maximumPreparationProducerCount = 4;
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(320, 320);
    QString errorString;
    std::vector<kiriview::DecodedImageResult> preparedResults;
    preparedResults.reserve(maximumPreparationProducerCount);
    for (std::size_t index = 0; index < maximumPreparationProducerCount; ++index) {
        auto source = std::make_shared<ResultDisplaySource>(QSize(4, 4));
        source->producerPeakByteCost = 128;
        source->blockingDisplay = testImage(QSize(4, 4));
        preparedResults.push_back(
            kiriview::staticDecodedImageResult(source, testDecodeRequest(), &errorString, budget));
        QVERIFY(staticDecodedImage(preparedResults.back()) != nullptr);
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(256));

    auto competingSource = std::make_shared<ResultDisplaySource>(QSize(4, 4));
    competingSource->producerPeakByteCost = 65;
    competingSource->blockingDisplay = testImage(QSize(4, 4));
    const kiriview::DecodedImageResult rejected = kiriview::staticDecodedImageResult(
        competingSource, testDecodeRequest(), &errorString, budget);
    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(rejected);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QCOMPARE(competingSource->firstDisplayDecodeCount, 0);
    QCOMPARE(competingSource->blockingDisplayDecodeCount, 0);
    QCOMPARE(budget->reservedByteCount(), qsizetype(256));

    preparedResults.erase(preparedResults.begin());
    QCOMPARE(budget->reservedByteCount(), qsizetype(192));

    const kiriview::DecodedImageResult admitted = kiriview::staticDecodedImageResult(
        competingSource, testDecodeRequest(), &errorString, budget);
    QVERIFY(staticDecodedImage(admitted) != nullptr);
    QCOMPARE(budget->reservedByteCount(), qsizetype(256));
}

void TestStaticImageDecode::staticResultRetainsAdmissionUntilLastPixelAliasRetires()
{
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(512, 512);
    auto source = std::make_shared<ResultDisplaySource>(QSize(4, 4));
    source->producerPeakByteCost = 256;
    source->blockingDisplay = testImage(QSize(4, 4));

    QImage retainedAlias;
    {
        QString errorString;
        const kiriview::DecodedImageResult result
            = kiriview::staticDecodedImageResult(source, testDecodeRequest(), &errorString, budget);
        const kiriview::StaticDecodedImage* decoded = staticDecodedImage(result);
        QVERIFY(decoded != nullptr);
        retainedAlias = decoded->displayImage.image;
        QCOMPARE(budget->reservedByteCount(), qsizetype(64));
    }
    source.reset();
    QCOMPARE(budget->reservedByteCount(), qsizetype(64));

    retainedAlias = {};
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestStaticImageDecode::decodeOnlyPayloadStillReportsRetainedRasterCost()
{
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(512, 512);
    auto source = std::make_shared<ResultDisplaySource>(QSize(4, 4));
    source->producerPeakByteCost = 256;
    source->blockingDisplay = testImage(QSize(4, 4));

    QString errorString;
    const kiriview::DecodedImageResult result = kiriview::staticDecodedImageResult(
        source, kiriview::ImageDecodeRequest {}, &errorString, budget);
    const kiriview::StaticDecodedImage* decoded = staticDecodedImage(result);

    QVERIFY(decoded != nullptr);
    QVERIFY(!decoded->displayImage.isValid());
    QCOMPARE(decoded->displayImage.retainedRasterByteCost(),
        static_cast<qsizetype>(decoded->displayImage.image.sizeInBytes()));
    QCOMPARE(budget->reservedByteCount(), decoded->displayImage.retainedRasterByteCost());
}

QTEST_GUILESS_MAIN(TestStaticImageDecode)

#include "tst_staticimagedecode.moc"
