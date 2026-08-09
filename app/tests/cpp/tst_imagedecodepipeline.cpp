// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodepipeline.h"

#include "decoding/imagedecodeworkspace.h"
#include "image_test_support.h"
#include "localization/imageerrortext.h"

#include <QByteArray>
#include <QByteArrayList>
#include <QList>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QTest>
#include <QUrl>
#include <memory>
#include <utility>

namespace {
kiriview::ImageInputClassification classification(kiriview::ImageInputKind kind,
    kiriview::QtRasterFormat qtFormat = kiriview::QtRasterFormat::None,
    kiriview::ImageDecodeDataSource dataSource = kiriview::ImageDecodeDataSource::Original)
{
    return kiriview::ImageInputClassification {
        kind,
        qtFormat,
        dataSource,
    };
}

kiriview::ImageDecodeRouterHandler recordingHandler(const QString& name, QStringList* calls,
    QByteArrayList* inputData = nullptr, QList<kiriview::QtRasterFormat>* qtFormats = nullptr,
    QString errorString = {})
{
    return [name, calls, inputData, qtFormats, errorString](
               const kiriview::ImageDecodeRouterInput& input) {
        calls->push_back(name);
        if (inputData != nullptr) {
            inputData->push_back(input.data);
        }
        if (qtFormats != nullptr) {
            qtFormats->push_back(input.qtRasterFormat);
        }
        return kiriview::failedDecodedImageResult(
            errorString.isEmpty() ? name + QStringLiteral(" failed") : errorString);
    };
}

kiriview::ImageDecodeRouterHandlers recordingHandlers(QStringList* calls,
    QByteArrayList* inputData = nullptr, QList<kiriview::QtRasterFormat>* qtFormats = nullptr)
{
    return kiriview::ImageDecodeRouterHandlers {
        recordingHandler(QStringLiteral("svg"), calls, inputData, qtFormats),
        recordingHandler(QStringLiteral("apng"), calls, inputData, qtFormats),
        recordingHandler(QStringLiteral("heif"), calls, inputData, qtFormats),
        recordingHandler(QStringLiteral("raw"), calls, inputData, qtFormats),
        recordingHandler(QStringLiteral("qt"), calls, inputData, qtFormats),
    };
}

QByteArray invalidJxlContainerData()
{
    QByteArray data;
    data.append(char(0x00));
    data.append(char(0x00));
    data.append(char(0x00));
    data.append(char(0x0c));
    data.append("JXL ", 4);
    data.append(char(0x0d));
    data.append(char(0x0a));
    data.append(char(0x87));
    data.append(char(0x0a));
    return data;
}

QByteArray jpegWithCameraMakeMetadata()
{
    return QByteArray::fromHex(
        "ffd8ffe1019045786966000049492a000800000007000f010200100000006200000010010200"
        "0a000000720000000e0102000e0000007c0000003c0102000100000000000000310102000e00"
        "00008a000000698704000100000098000000258804000100000022010000000000004b697269"
        "2043616d65726120436f2e004b69726943616d203100416476616e636564206e6f7465004b69"
        "72694f532043616d6572610006000390020014000000e600000034a4020010000000fa000000"
        "9a820500010000000a0100009d82050001000000120100002788030001000000900100000a92"
        "0500010000001a01000000000000323032363a30353a33312031323a33343a3536004b697269"
        "205072696d652033356d6d00010000007d000000380000000a00000023000000010000000400"
        "01000200020000004e0000000200050003000000580100000300020002000000570000000400"
        "050003000000700100000000000025000000010000002e00000001000000940b000064000000"
        "7a000000010000001900000001000000d803000064000000ffd9");
}
}

class TestImageDecodePipeline : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void routePlanKeepsClassificationSeparateFromDecoderExecution();
    void runtimeExecutesRoutePlansWithoutClassifier();
    void routerCallsExactlyOneDecoderForClassifiedInputs();
    void selectedDecoderFailureDoesNotFallback();
    void metadataWorkspaceAdmissionIsOptionalAndReleased();
    void compatibleDataIsComputedOnlyWhenClassificationRequestsIt();
    void qtRasterClassificationCarriesExplicitFormat();
    void defaultSvgDecodeUsesFirstDisplayContext();
    void defaultSvgOpenFailurePreservesAdapterDiagnostics();
    void defaultApngOpenFailurePreservesAdapterDiagnostics();
    void defaultJxlAnimationOpenFailurePreservesAdapterDiagnostics();
    void unknownClassificationFailsWithoutDecoder();
};

void TestImageDecodePipeline::routePlanKeepsClassificationSeparateFromDecoderExecution()
{
    struct Case
    {
        kiriview::ImageInputClassification classification;
        kiriview::ImageDecodeHandlerKind expectedHandler;
        kiriview::ImageDecodeDataSource expectedDataSource;
        kiriview::QtRasterFormat expectedFormat;
        bool expectedShouldDecode = true;
    };
    const QList<Case> cases {
        { classification(kiriview::ImageInputKind::Svg), kiriview::ImageDecodeHandlerKind::Svg,
            kiriview::ImageDecodeDataSource::Original, kiriview::QtRasterFormat::None },
        { classification(kiriview::ImageInputKind::Apng), kiriview::ImageDecodeHandlerKind::Apng,
            kiriview::ImageDecodeDataSource::Original, kiriview::QtRasterFormat::None },
        { classification(kiriview::ImageInputKind::HeifFamily, kiriview::QtRasterFormat::None,
              kiriview::ImageDecodeDataSource::AvifCompatible),
            kiriview::ImageDecodeHandlerKind::HeifFamily,
            kiriview::ImageDecodeDataSource::AvifCompatible, kiriview::QtRasterFormat::None },
        { classification(kiriview::ImageInputKind::Raw), kiriview::ImageDecodeHandlerKind::Raw,
            kiriview::ImageDecodeDataSource::Original, kiriview::QtRasterFormat::None },
        { classification(kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Jxl),
            kiriview::ImageDecodeHandlerKind::QtRaster, kiriview::ImageDecodeDataSource::Original,
            kiriview::QtRasterFormat::Jxl },
        { classification(kiriview::ImageInputKind::Unknown, kiriview::QtRasterFormat::None,
              kiriview::ImageDecodeDataSource::AvifCompatible),
            kiriview::ImageDecodeHandlerKind::None, kiriview::ImageDecodeDataSource::Original,
            kiriview::QtRasterFormat::None, false },
    };

    for (const Case& testCase : cases) {
        const kiriview::ImageDecodeRoute route
            = kiriview::imageDecodeRouteForClassification(testCase.classification);
        QCOMPARE(route.handlerKind, testCase.expectedHandler);
        QCOMPARE(route.dataSource, testCase.expectedDataSource);
        QCOMPARE(route.qtRasterFormat, testCase.expectedFormat);
        QCOMPARE(route.shouldDecode(), testCase.expectedShouldDecode);
    }
}

void TestImageDecodePipeline::runtimeExecutesRoutePlansWithoutClassifier()
{
    const QByteArray originalData = QByteArrayLiteral("original image bytes");
    const QByteArray compatibleData = QByteArrayLiteral("compatible image bytes");

    int compatibleTransformCount = 0;
    QStringList calls;
    QByteArrayList inputData;
    QList<kiriview::QtRasterFormat> qtFormats;
    kiriview::ImageDecodeRouterRuntime runtime(recordingHandlers(&calls, &inputData, &qtFormats),
        [&compatibleTransformCount, compatibleData](const QByteArray&) {
            ++compatibleTransformCount;
            return compatibleData;
        });

    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::QtRaster,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::Jxl,
    };

    (void)runtime.execute(route, originalData, kiriview::ImageDecodeRequest {});

    QCOMPARE(compatibleTransformCount, 1);
    QCOMPARE(calls, QStringList({ QStringLiteral("qt") }));
    QCOMPARE(inputData, QByteArrayList({ compatibleData }));
    QCOMPARE(qtFormats, QList<kiriview::QtRasterFormat>({ kiriview::QtRasterFormat::Jxl }));
}

void TestImageDecodePipeline::routerCallsExactlyOneDecoderForClassifiedInputs()
{
    struct Case
    {
        kiriview::ImageInputClassification classification;
        QString expectedCall;
    };
    const QList<Case> cases {
        { classification(kiriview::ImageInputKind::Svg), QStringLiteral("svg") },
        { classification(kiriview::ImageInputKind::Apng), QStringLiteral("apng") },
        { classification(kiriview::ImageInputKind::HeifFamily), QStringLiteral("heif") },
        { classification(kiriview::ImageInputKind::Raw), QStringLiteral("raw") },
        { classification(kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Png),
            QStringLiteral("qt") },
    };

    for (const Case& testCase : cases) {
        QStringList calls;
        kiriview::ImageDecodeRouter router(recordingHandlers(&calls),
            [testCase](const QByteArray&, const QString&) { return testCase.classification; });

        const kiriview::DecodedImageResult result
            = router.decode(QByteArrayLiteral("image bytes"), kiriview::ImageDecodeRequest {});

        QVERIFY(kiriview::decodedImageResultFailure(result) != nullptr);
        QCOMPARE(calls, QStringList({ testCase.expectedCall }));
    }
}

void TestImageDecodePipeline::selectedDecoderFailureDoesNotFallback()
{
    QStringList calls;
    kiriview::ImageDecodeRouterHandlers handlers = recordingHandlers(&calls);
    handlers.raw = recordingHandler(
        QStringLiteral("raw"), &calls, nullptr, nullptr, QStringLiteral("raw failed"));
    handlers.qtRaster = recordingHandler(
        QStringLiteral("qt"), &calls, nullptr, nullptr, QStringLiteral("qt fallback ran"));
    kiriview::ImageDecodeRouter router(std::move(handlers), [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::Raw);
    });

    const kiriview::DecodedImageResult result
        = router.decode(QByteArrayLiteral("not raw"), kiriview::ImageDecodeRequest {});

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->errorString, QStringLiteral("raw failed"));
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::Raw);
    QCOMPARE(calls, QStringList({ QStringLiteral("raw") }));
}

void TestImageDecodePipeline::metadataWorkspaceAdmissionIsOptionalAndReleased()
{
    kiriview::ImageDecodeRouterHandlers handlers;
    handlers.qtRaster = [](const kiriview::ImageDecodeRouterInput&) {
        return kiriview::successfulDecodedImageResult(
            kiriview::TestSupport::staticDecodedTestImage());
    };
    const auto classifier = [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Jpeg);
    };
    kiriview::ImageDecodeRouter router(std::move(handlers), classifier);
    const QByteArray data = jpegWithCameraMakeMetadata();

    auto admittedBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        128 * 1024 * 1024, 128 * 1024 * 1024);
    const kiriview::DecodedImageResult admitted
        = router.decode(data, kiriview::ImageDecodeRequest {}, admittedBudget);
    const kiriview::DecodedImage* admittedImage = kiriview::decodedImageResultImage(admitted);
    QVERIFY(admittedImage != nullptr);
    QCOMPARE(kiriview::decodedImageEmbeddedMetadata(*admittedImage).cameraMake,
        QStringLiteral("Kiri Camera Co."));
    QCOMPARE(admittedBudget->reservedByteCount(), qsizetype(0));

    auto unavailableBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(1024, 1024);
    const kiriview::DecodedImageResult unavailable
        = router.decode(data, kiriview::ImageDecodeRequest {}, unavailableBudget);
    const kiriview::DecodedImage* unavailableImage = kiriview::decodedImageResultImage(unavailable);
    QVERIFY(unavailableImage != nullptr);
    QVERIFY(kiriview::decodedImageEmbeddedMetadata(*unavailableImage).isEmpty());
    QCOMPARE(unavailableBudget->reservedByteCount(), qsizetype(0));

    constexpr qsizetype existingWorkspaceByteCost = 1024 * 1024;
    constexpr qsizetype metadataWorkspaceByteCost = 64 * 1024 * 1024;
    kiriview::ImageDecodeRouterHandlers animationHandlers;
    animationHandlers.apng = [existingWorkspaceByteCost](
                                 const kiriview::ImageDecodeRouterInput& input) {
        kiriview::ImageDecodeWorkspaceLease lease = input.workspaceBudget->startLease();
        if (!lease.tryReserve(existingWorkspaceByteCost)) {
            return kiriview::failedDecodedImageResult(QStringLiteral("workspace unavailable"));
        }
        kiriview::ImageDecodeWorkspaceHold hold = lease.retainOnly(existingWorkspaceByteCost);
        return kiriview::successfulDecodedImageResult(kiriview::ApngAnimationImage {
            std::move(hold), kiriview::TestSupport::testImage(), {}, {}, {}, {}, {}, {} });
    };
    kiriview::ImageDecodeRouter animationRouter(
        std::move(animationHandlers), [](const QByteArray&, const QString&) {
            return classification(kiriview::ImageInputKind::Apng);
        });
    auto operationLimitedBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        256 * 1024 * 1024, existingWorkspaceByteCost + metadataWorkspaceByteCost - 1);
    {
        const kiriview::DecodedImageResult operationLimited
            = animationRouter.decode(data, kiriview::ImageDecodeRequest {}, operationLimitedBudget);
        const kiriview::DecodedImage* operationLimitedImage
            = kiriview::decodedImageResultImage(operationLimited);
        QVERIFY(operationLimitedImage != nullptr);
        QVERIFY(kiriview::decodedImageEmbeddedMetadata(*operationLimitedImage).isEmpty());
        QCOMPARE(operationLimitedBudget->reservedByteCount(), existingWorkspaceByteCost);
    }
    QCOMPARE(operationLimitedBudget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::compatibleDataIsComputedOnlyWhenClassificationRequestsIt()
{
    const QByteArray originalData = QByteArrayLiteral("original image bytes");
    const QByteArray compatibleData = QByteArrayLiteral("compatible image bytes");

    int qtCompatibleTransformCount = 0;
    QStringList qtCalls;
    QByteArrayList qtInputData;
    kiriview::ImageDecodeRouter qtRouter(
        recordingHandlers(&qtCalls, &qtInputData),
        [](const QByteArray&, const QString&) {
            return classification(
                kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Png);
        },
        [&qtCompatibleTransformCount, compatibleData](const QByteArray&) {
            ++qtCompatibleTransformCount;
            return compatibleData;
        });

    (void)qtRouter.decode(originalData, kiriview::ImageDecodeRequest {});

    QCOMPARE(qtCompatibleTransformCount, 0);
    QCOMPARE(qtCalls, QStringList({ QStringLiteral("qt") }));
    QCOMPARE(qtInputData, QByteArrayList({ originalData }));

    int heifCompatibleTransformCount = 0;
    QStringList heifCalls;
    QByteArrayList heifInputData;
    kiriview::ImageDecodeRouter heifRouter(
        recordingHandlers(&heifCalls, &heifInputData),
        [](const QByteArray&, const QString&) {
            return classification(kiriview::ImageInputKind::HeifFamily,
                kiriview::QtRasterFormat::None, kiriview::ImageDecodeDataSource::AvifCompatible);
        },
        [&heifCompatibleTransformCount, compatibleData](const QByteArray&) {
            ++heifCompatibleTransformCount;
            return compatibleData;
        });

    (void)heifRouter.decode(originalData, kiriview::ImageDecodeRequest {});

    QCOMPARE(heifCompatibleTransformCount, 1);
    QCOMPARE(heifCalls, QStringList({ QStringLiteral("heif") }));
    QCOMPARE(heifInputData, QByteArrayList({ compatibleData }));
}

void TestImageDecodePipeline::qtRasterClassificationCarriesExplicitFormat()
{
    const QList<kiriview::QtRasterFormat> formats {
        kiriview::QtRasterFormat::Png,
        kiriview::QtRasterFormat::Jpeg,
        kiriview::QtRasterFormat::Gif,
        kiriview::QtRasterFormat::Webp,
        kiriview::QtRasterFormat::Bmp,
        kiriview::QtRasterFormat::Tiff,
        kiriview::QtRasterFormat::Jxl,
        kiriview::QtRasterFormat::Jp2,
    };

    for (kiriview::QtRasterFormat format : formats) {
        QStringList calls;
        QList<kiriview::QtRasterFormat> qtFormats;
        kiriview::ImageDecodeRouter router(recordingHandlers(&calls, nullptr, &qtFormats),
            [format](const QByteArray&, const QString&) {
                return classification(kiriview::ImageInputKind::QtRaster, format);
            });

        (void)router.decode(QByteArrayLiteral("raster bytes"), kiriview::ImageDecodeRequest {});

        QCOMPARE(calls, QStringList({ QStringLiteral("qt") }));
        QCOMPARE(qtFormats, QList<kiriview::QtRasterFormat>({ format }));
    }
}

void TestImageDecodePipeline::defaultSvgDecodeUsesFirstDisplayContext()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");
    const kiriview::DecodedImageResult result = kiriview::decodeImageDataWithDefaultRouter(data,
        kiriview::ImageDecodeRequest::fromUrl(7, QUrl(QStringLiteral("file:///tmp/vector.svg")),
            kiriview::ImageFirstDisplayDecodeContext { QSize(200, 200) }));

    const auto* image = kiriview::decodedImageResultImageAs<kiriview::StaticDecodedImage>(result);
    QVERIFY(image != nullptr);
    QCOMPARE(image->displayImage.image.size(), QSize(200, 100));
    QCOMPARE(image->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);
    QCOMPARE(image->displayImage.sourceDetailModel,
        kiriview::StaticImageSourceDetailModel::ScalableRasterization);
}

void TestImageDecodePipeline::defaultSvgOpenFailurePreservesAdapterDiagnostics()
{
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::Svg,
        kiriview::ImageDecodeDataSource::Original,
        kiriview::QtRasterFormat::None,
    };
    const kiriview::ImageDecodeRouterRuntime runtime({});

    const kiriview::DecodedImageResult result
        = runtime.execute(route, QByteArrayLiteral("<svg"), kiriview::ImageDecodeRequest {});

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::Svg);
    QVERIFY(failure->operation != kiriview::DecodedImageFailureOperation::Unknown);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QVERIFY(failure->diagnosticDetail != failure->errorString);
    QVERIFY(kiriview::decodedImageResultImage(result) == nullptr);
}

void TestImageDecodePipeline::defaultApngOpenFailurePreservesAdapterDiagnostics()
{
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::Apng,
        kiriview::ImageDecodeDataSource::Original,
        kiriview::QtRasterFormat::None,
    };
    const kiriview::ImageDecodeRouterRuntime runtime({});

    const kiriview::DecodedImageResult result
        = runtime.execute(route, QByteArrayLiteral("not an apng"), kiriview::ImageDecodeRequest {});

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::Apng);
    QVERIFY(failure->operation != kiriview::DecodedImageFailureOperation::Unknown);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QVERIFY(failure->diagnosticDetail != failure->errorString);
    QVERIFY(kiriview::decodedImageResultImage(result) == nullptr);
}

void TestImageDecodePipeline::defaultJxlAnimationOpenFailurePreservesAdapterDiagnostics()
{
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::QtRaster,
        kiriview::ImageDecodeDataSource::Original,
        kiriview::QtRasterFormat::Jxl,
    };
    const kiriview::ImageDecodeRouterRuntime runtime({});

    const kiriview::DecodedImageResult result
        = runtime.execute(route, invalidJxlContainerData(), kiriview::ImageDecodeRequest {});

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::QtRaster);
    QVERIFY(failure->operation != kiriview::DecodedImageFailureOperation::Unknown);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QVERIFY(failure->diagnosticDetail != failure->errorString);
    QVERIFY(kiriview::decodedImageResultImage(result) == nullptr);
}

void TestImageDecodePipeline::unknownClassificationFailsWithoutDecoder()
{
    int compatibleTransformCount = 0;
    QStringList calls;
    kiriview::ImageDecodeRouter router(
        recordingHandlers(&calls),
        [](const QByteArray&, const QString&) {
            return classification(kiriview::ImageInputKind::Unknown, kiriview::QtRasterFormat::None,
                kiriview::ImageDecodeDataSource::AvifCompatible);
        },
        [&compatibleTransformCount](const QByteArray& data) {
            ++compatibleTransformCount;
            return data + QByteArrayLiteral("-compatible");
        });

    const kiriview::DecodedImageResult result
        = router.decode(QByteArrayLiteral("unknown bytes"), kiriview::ImageDecodeRequest {});

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(
        failure->errorString, kiriview::imageErrorText(kiriview::ImageErrorTextId::ReadImageData));
    QVERIFY(calls.isEmpty());
    QCOMPARE(compatibleTransformCount, 0);
}

QTEST_GUILESS_MAIN(TestImageDecodePipeline)

#include "tst_imagedecodepipeline.moc"
