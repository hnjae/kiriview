// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/decodedimageresult.h"

#include "decoding/imagedecodeworkspace.h"
#include "image_test_support.h"

#include <QImage>
#include <QObject>
#include <QTest>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

class TestDecodedImageResult : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void exposesFailurePayload();
    void exposesImagePayload();
    void takeImageMovesImagePayloadOnly();
    void staticMetadataMirrorsIntoDisplayPayload();
    void animationAssignmentDestroysImageBeforeWorkspaceHold();
};

namespace {
struct ImageCleanupObservation
{
    std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget> budget;
    bool called = false;
    qsizetype reservedByteCount = 0;
};

void observeImageCleanup(void* data)
{
    auto* observation = static_cast<ImageCleanupObservation*>(data);
    observation->called = true;
    observation->reservedByteCount = observation->budget->reservedByteCount();
}
}

void TestDecodedImageResult::exposesFailurePayload()
{
    const kiriview::DecodedImageResult result
        = kiriview::failedDecodedImageResult(QStringLiteral("decode failed"));

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->errorString, QStringLiteral("decode failed"));
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::Unknown);
    QCOMPARE(failure->operation, kiriview::DecodedImageFailureOperation::Unknown);
    QCOMPARE(failure->diagnosticDetail, QStringLiteral("decode failed"));
    QCOMPARE(failure->severity, kiriview::DecodedImageFailureSeverity::Error);
    QVERIFY(!failure->retryable);
    QVERIFY(kiriview::decodedImageResultImage(result) == nullptr);
}

void TestDecodedImageResult::exposesImagePayload()
{
    const kiriview::DecodedImageResult result
        = kiriview::successfulDecodedImageResult(kiriview::TestSupport::staticDecodedTestImage());

    QVERIFY(kiriview::decodedImageResultFailure(result) == nullptr);
    const kiriview::DecodedImage* image = kiriview::decodedImageResultImage(result);
    QVERIFY(image != nullptr);
    QVERIFY(std::get_if<kiriview::StaticDecodedImage>(image) != nullptr);
}

void TestDecodedImageResult::takeImageMovesImagePayloadOnly()
{
    kiriview::DecodedImageResult result
        = kiriview::successfulDecodedImageResult(kiriview::TestSupport::staticDecodedTestImage());
    std::optional<kiriview::DecodedImage> image;
    if (result) {
        image = std::move(*result);
    }
    QVERIFY(image.has_value());
    QVERIFY(std::get_if<kiriview::StaticDecodedImage>(&*image) != nullptr);

    result = kiriview::failedDecodedImageResult(QStringLiteral("decode failed"));
    image = result ? std::optional<kiriview::DecodedImage>(std::move(*result)) : std::nullopt;
    QVERIFY(!image.has_value());
}

void TestDecodedImageResult::staticMetadataMirrorsIntoDisplayPayload()
{
    kiriview::DecodedImage image = kiriview::TestSupport::staticDecodedTestImage();
    kiriview::EmbeddedMetadata metadata;
    metadata.cameraMake = QStringLiteral("Kiri Camera");

    kiriview::setDecodedImageEmbeddedMetadata(image, metadata);

    const kiriview::StaticDecodedImage* decoded = std::get_if<kiriview::StaticDecodedImage>(&image);
    QVERIFY(decoded != nullptr);
    QCOMPARE(
        kiriview::decodedImageEmbeddedMetadata(image).cameraMake, QStringLiteral("Kiri Camera"));
    QCOMPARE(decoded->displayImage.embeddedMetadata.cameraMake, QStringLiteral("Kiri Camera"));
}

void TestDecodedImageResult::animationAssignmentDestroysImageBeforeWorkspaceHold()
{
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(4, 4);
    kiriview::ImageDecodeWorkspaceLease lease = budget->startLease();
    QVERIFY(lease.tryReserve(4));
    ImageCleanupObservation observation { budget };
    std::array<uchar, 4> pixels {};
    QImage image(
        pixels.data(), 1, 1, 4, QImage::Format_RGBA8888, observeImageCleanup, &observation);
    QVERIFY(!image.isNull());

    kiriview::ReaderAnimationImage decoded {
        lease.sharedHold(),
        std::move(image),
    };
    lease = {};
    QCOMPARE(budget->reservedByteCount(), qsizetype(4));

    decoded = kiriview::ReaderAnimationImage {};

    QVERIFY(observation.called);
    QCOMPARE(observation.reservedByteCount, qsizetype(4));
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

QTEST_GUILESS_MAIN(TestDecodedImageResult)

#include "tst_decodedimageresult.moc"
