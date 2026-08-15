// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodepipeline.h"

#include "decoding/heifdisplaysource.h"
#include "decoding/heifsequencereader.h"
#include "decoding/imagedecodeworkspace.h"
#include "decoding/jxlanimationreader.h"
#include "decoding/rawdecoder.h"
#include "decoding/svgdisplaysource.h"
#include "image_test_support.h"
#include "localization/imageerrortext.h"

#include <QBuffer>
#include <QByteArray>
#include <QByteArrayList>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QList>
#include <QObject>
#include <QSize>
#include <QStringList>
#include <QTest>
#include <QUrl>
#include <limits>
#include <memory>
#include <optional>
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

QByteArray fixtureData(const QString& fileName)
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/") + fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QByteArray animatedGifData()
{
    return QByteArray::fromBase64(
        QByteArrayLiteral("R0lGODlhAQABAIAAAP8AAAAA/yH/C05FVFNDQVBFMi4wAwECAAAh+QQAAQAAACwAAAAAAQAB"
                          "AAACAkQBACH5BAACAAAALAAAAAABAAEAAAICTAEAOw=="));
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

QByteArray encodedImageData(const QImage& image, const QByteArray& format, QString* errorString)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format);
    if (!writer.write(image)) {
        if (errorString != nullptr) {
            *errorString = writer.errorString();
        }
        return {};
    }
    return data;
}

std::optional<kiriview::PreparedImageDecodeResult> executePreparedWork(
    std::unique_ptr<kiriview::PreparedImageDecodeWork> work,
    const std::shared_ptr<kiriview::ImageDecodeWorkspaceBudget>& budget)
{
    if (work == nullptr) {
        return std::nullopt;
    }
    const kiriview::ImageDecodeWorkspaceAdmissionRequest request = work->admissionRequest();
    kiriview::ImageDecodeWorkspaceLease lease
        = kiriview::ImageDecodeWorkspaceDetail::startLeaseForOperation(
            *budget, request.perOperationBaselineByteCount);
    if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(lease, request.additionalPeakByteCount)) {
        return std::nullopt;
    }
    return std::move(*work).execute(std::move(lease));
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
    void compatibleDataRequiresWorkspaceAdmissionBeforeTransform();
    void compatibleDataCapacityCannotExceedPreflight();
    void compatibleDataOwnedStorageRejectsNonOwningView();
    void compatibleDataSharingOriginalStorageReleasesWorkspaceBeforeDecode();
    void compatibleDataWorkspaceRemainsReservedUntilDecodedImageRelease();
    void compatibleDataAdmissionIsSharedAcrossLiveResults();
    void preparedCompatibleDataCarriesRetainedBaselineIntoExactRasterStage();
    void qtRasterClassificationCarriesExplicitFormat();
    void preparedQtRasterStillDeclaresExactProducerEnvelope();
    void defaultSvgDecodeUsesFirstDisplayContext();
    void preparedSvgSeparatesParserAndRasterEnvelopes();
    void preparedRawCarriesOpenHoldIntoExactProduction();
    void preparedRawCompatibleInputBaselineIncludesLiveOpenHold();
    void preparedHeifSequenceSeparatesProbeAndProduction();
    void preparedRawAndHeifHardLimitsPreserveTypedRoutes_data();
    void preparedRawAndHeifHardLimitsPreserveTypedRoutes();
    void preparedDefaultAnimationDeclaresBoundedEnvelope_data();
    void preparedDefaultAnimationDeclaresBoundedEnvelope();
    void preparedJxlAnimationUsesPrechargedAllocatorEnvelope();
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
        kiriview::ImageDecodeCompatibleDataTransform {
            [compatibleData](qsizetype) { return std::optional(compatibleData.size()); },
            [&compatibleTransformCount, compatibleData](const QByteArray&) {
                ++compatibleTransformCount;
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    QByteArray(compatibleData.constData(), compatibleData.size()),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
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
        kiriview::ImageDecodeWorkspaceLease lease
            = kiriview::ImageDecodeWorkspaceDetail::startLease(*input.workspaceBudget);
        if (!kiriview::ImageDecodeWorkspaceDetail::tryReserve(lease, existingWorkspaceByteCost)) {
            return kiriview::failedDecodedImageResult(QStringLiteral("workspace unavailable"));
        }
        kiriview::ImageDecodeWorkspaceHold hold = lease.retainOnly(existingWorkspaceByteCost);
        return kiriview::successfulDecodedImageResult(kiriview::ApngAnimationImage {
            std::move(hold), kiriview::TestSupport::testImage(), {}, {}, {}, {}, {}, {}, {} });
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
        kiriview::ImageDecodeCompatibleDataTransform {
            [compatibleData](qsizetype) { return std::optional(compatibleData.size()); },
            [&qtCompatibleTransformCount, compatibleData](const QByteArray&) {
                ++qtCompatibleTransformCount;
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    QByteArray(compatibleData.constData(), compatibleData.size()),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
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
        kiriview::ImageDecodeCompatibleDataTransform {
            [compatibleData](qsizetype) { return std::optional(compatibleData.size()); },
            [&heifCompatibleTransformCount, compatibleData](const QByteArray&) {
                ++heifCompatibleTransformCount;
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    QByteArray(compatibleData.constData(), compatibleData.size()),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });

    (void)heifRouter.decode(originalData, kiriview::ImageDecodeRequest {});

    QCOMPARE(heifCompatibleTransformCount, 1);
    QCOMPARE(heifCalls, QStringList({ QStringLiteral("heif") }));
    QCOMPARE(heifInputData, QByteArrayList({ compatibleData }));
}

void TestImageDecodePipeline::compatibleDataRequiresWorkspaceAdmissionBeforeTransform()
{
    const QByteArray originalData = QByteArrayLiteral("original image bytes");
    int compatibleTransformCount = 0;
    QStringList calls;
    kiriview::ImageDecodeRouterRuntime runtime(recordingHandlers(&calls),
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) -> std::optional<qsizetype> {
                if (sourceByteCount > std::numeric_limits<qsizetype>::max() / 2) {
                    return std::nullopt;
                }
                return sourceByteCount * 2;
            },
            [&compatibleTransformCount](const QByteArray& data) {
                ++compatibleTransformCount;
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    data + data,
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::HeifFamily,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::None,
    };
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        originalData.size(), originalData.size());

    const kiriview::DecodedImageResult result
        = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::HeifFamily);
    QCOMPARE(compatibleTransformCount, 0);
    QVERIFY(calls.isEmpty());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::compatibleDataCapacityCannotExceedPreflight()
{
    const QByteArray originalData = QByteArrayLiteral("original image bytes");
    int compatibleTransformCount = 0;
    QStringList calls;
    kiriview::ImageDecodeRouterRuntime runtime(recordingHandlers(&calls),
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) { return std::optional(sourceByteCount); },
            [&compatibleTransformCount](const QByteArray& data) {
                ++compatibleTransformCount;
                QByteArray replacement(data.constData(), data.size());
                replacement.reserve(data.size() * 2);
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    std::move(replacement),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::HeifFamily,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::None,
    };
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        originalData.size(), originalData.size());

    const kiriview::DecodedImageResult result
        = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QCOMPARE(compatibleTransformCount, 1);
    QVERIFY(calls.isEmpty());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::compatibleDataOwnedStorageRejectsNonOwningView()
{
    const QByteArray originalData = QByteArrayLiteral("original image bytes");
    QStringList calls;
    kiriview::ImageDecodeRouterRuntime runtime(recordingHandlers(&calls),
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) { return std::optional(sourceByteCount); },
            [](const QByteArray& data) {
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    QByteArray::fromRawData(data.constData(), data.size()),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::HeifFamily,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::None,
    };
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        originalData.size(), originalData.size());

    const kiriview::DecodedImageResult result
        = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);

    const kiriview::DecodedImageFailure* failure = kiriview::decodedImageResultFailure(result);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QVERIFY(calls.isEmpty());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::compatibleDataSharingOriginalStorageReleasesWorkspaceBeforeDecode()
{
    const QByteArray originalData = QByteArrayLiteral("unchanged image bytes");
    bool workspaceReleasedBeforeDecode = false;
    kiriview::ImageDecodeRouterHandlers handlers;
    handlers.heifFamily
        = [&workspaceReleasedBeforeDecode](const kiriview::ImageDecodeRouterInput& input) {
              workspaceReleasedBeforeDecode = input.workspaceBudget->reservedByteCount() == 0;
              return kiriview::successfulDecodedImageResult(
                  kiriview::TestSupport::staticDecodedTestImage());
          };
    kiriview::ImageDecodeRouterRuntime runtime(std::move(handlers),
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) { return std::optional(sourceByteCount); },
            [](const QByteArray& data) {
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    data,
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::Original,
                };
            },
        });
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::HeifFamily,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::None,
    };
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        originalData.size(), originalData.size());

    const kiriview::DecodedImageResult result
        = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);

    QVERIFY(kiriview::decodedImageResultImage(result) != nullptr);
    QVERIFY(workspaceReleasedBeforeDecode);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::compatibleDataWorkspaceRemainsReservedUntilDecodedImageRelease()
{
    const QByteArray originalData = QByteArrayLiteral("original image bytes");
    bool retainedInputBaselineReported = false;
    qsizetype retainedInputByteCount = 0;
    kiriview::ImageDecodeRouterHandlers handlers;
    handlers.heifFamily = [&retainedInputBaselineReported, &retainedInputByteCount](
                              const kiriview::ImageDecodeRouterInput& input) {
        retainedInputByteCount = input.data.capacity();
        retainedInputBaselineReported
            = input.retainedInputWorkspaceByteCount == retainedInputByteCount;
        return kiriview::successfulDecodedImageResult(kiriview::HeifSequenceAnimationImage {
            {}, kiriview::TestSupport::testImage(), {}, {}, input.data, {}, {}, {}, {} });
    };
    kiriview::ImageDecodeRouterRuntime runtime(std::move(handlers),
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) { return std::optional(sourceByteCount * 2); },
            [](const QByteArray& data) {
                QByteArray replacement(data.constData(), data.size());
                replacement.reserve(data.size() * 2);
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    std::move(replacement),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::HeifFamily,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::None,
    };
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        originalData.size() * 2, originalData.size() * 2);

    {
        const kiriview::DecodedImageResult result
            = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);

        const auto* image
            = kiriview::decodedImageResultImageAs<kiriview::HeifSequenceAnimationImage>(result);
        QVERIFY(image != nullptr);
        QCOMPARE(image->data, originalData);
        QVERIFY(image->data.constData() != originalData.constData());
        QVERIFY(retainedInputBaselineReported);
        QVERIFY(retainedInputByteCount > originalData.size());
        QCOMPARE(budget->reservedByteCount(), retainedInputByteCount);
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::compatibleDataAdmissionIsSharedAcrossLiveResults()
{
    const QByteArray originalData = QByteArrayLiteral("aggregate compatible image bytes");
    int compatibleTransformCount = 0;
    kiriview::ImageDecodeRouterHandlers handlers;
    handlers.heifFamily = [](const kiriview::ImageDecodeRouterInput& input) {
        return kiriview::successfulDecodedImageResult(kiriview::HeifSequenceAnimationImage {
            {}, kiriview::TestSupport::testImage(), {}, {}, input.data, {}, {}, {}, {} });
    };
    kiriview::ImageDecodeRouterRuntime runtime(std::move(handlers),
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) { return std::optional(sourceByteCount); },
            [&compatibleTransformCount](const QByteArray& data) {
                ++compatibleTransformCount;
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    QByteArray(data.constData(), data.size()),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    const kiriview::ImageDecodeRoute route {
        kiriview::ImageDecodeHandlerKind::HeifFamily,
        kiriview::ImageDecodeDataSource::AvifCompatible,
        kiriview::QtRasterFormat::None,
    };
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        originalData.size(), originalData.size());

    {
        const kiriview::DecodedImageResult first
            = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);
        QVERIFY(kiriview::decodedImageResultImage(first) != nullptr);
        QCOMPARE(compatibleTransformCount, 1);
        QCOMPARE(budget->reservedByteCount(), originalData.size());

        const kiriview::DecodedImageResult concurrent
            = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);
        const kiriview::DecodedImageFailure* failure
            = kiriview::decodedImageResultFailure(concurrent);
        QVERIFY(failure != nullptr);
        QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
        QCOMPARE(compatibleTransformCount, 1);
        QCOMPARE(budget->reservedByteCount(), originalData.size());
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));

    {
        const kiriview::DecodedImageResult admittedAfterRelease
            = runtime.execute(route, originalData, kiriview::ImageDecodeRequest {}, budget);
        QVERIFY(kiriview::decodedImageResultImage(admittedAfterRelease) != nullptr);
        QCOMPARE(compatibleTransformCount, 2);
        QCOMPARE(budget->reservedByteCount(), originalData.size());
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::preparedCompatibleDataCarriesRetainedBaselineIntoExactRasterStage()
{
    QImage sourceImage(QSize(4, 3), QImage::Format_RGBA8888);
    sourceImage.fill(Qt::red);
    QString errorString;
    const QByteArray originalData
        = encodedImageData(sourceImage, QByteArrayLiteral("png"), &errorString);
    QVERIFY2(!originalData.isEmpty(), qPrintable(errorString));

    constexpr qsizetype transformHeadroom = 4096;
    int transformCount = 0;
    qsizetype retainedInputByteCount = 0;
    kiriview::ImageDecodeRouter router(
        {},
        [](const QByteArray&, const QString&) {
            return classification(kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Png,
                kiriview::ImageDecodeDataSource::AvifCompatible);
        },
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) {
                return std::optional(sourceByteCount + transformHeadroom);
            },
            [&transformCount, &retainedInputByteCount](const QByteArray& data) {
                ++transformCount;
                QByteArray replacement(data.constData(), data.size());
                replacement.reserve(data.size() + 1024);
                retainedInputByteCount = replacement.capacity();
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    std::move(replacement),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    constexpr qsizetype workspaceLimit = 1024 * 1024;
    auto workspaceBudget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    auto sourceBudget = std::make_shared<kiriview::ImageSourceDataBudget>(
        originalData.size(), originalData.size());
    kiriview::ImageSourceDataLease sourceLease = sourceBudget->startLease();
    QVERIFY(sourceLease.tryReserve(originalData.size()));
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(
        8, QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-compatible.png")));

    kiriview::PreparedImageDecodeResult transformPlan
        = router.prepare(kiriview::ImageSourceData(originalData, std::move(sourceLease)), request,
            kiriview::ImageDecodeWorkspacePriority::Demanded, workspaceBudget);
    auto* transformWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&transformPlan);
    QVERIFY(transformWork != nullptr);
    QVERIFY(*transformWork != nullptr);
    QCOMPARE(transformCount, 0);
    QCOMPARE(transformWork->get()->admissionRequest().additionalPeakByteCount,
        originalData.size() + transformHeadroom);
    QCOMPARE(transformWork->get()->admissionRequest().perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(transformWork->get()->admissionRequest().priority,
        kiriview::ImageDecodeWorkspacePriority::Demanded);
    QCOMPARE(workspaceBudget->reservedByteCount(), qsizetype(0));

    std::optional<kiriview::PreparedImageDecodeResult> rasterPlan
        = executePreparedWork(std::move(*transformWork), workspaceBudget);
    QVERIFY(rasterPlan.has_value());
    QCOMPARE(transformCount, 1);
    QVERIFY(retainedInputByteCount > 0);
    QCOMPARE(workspaceBudget->reservedByteCount(), retainedInputByteCount);
    auto* rasterWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*rasterPlan);
    QVERIFY(rasterWork != nullptr);
    QVERIFY(*rasterWork != nullptr);
    const kiriview::ImageDecodeWorkspaceAdmissionRequest rasterAdmission
        = rasterWork->get()->admissionRequest();
    QCOMPARE(rasterAdmission.perOperationBaselineByteCount, retainedInputByteCount);
    QVERIFY(rasterAdmission.additionalPeakByteCount > 0);
    QVERIFY(rasterAdmission.additionalPeakByteCount < workspaceLimit - retainedInputByteCount);
    QCOMPARE(rasterAdmission.priority, kiriview::ImageDecodeWorkspacePriority::Demanded);

    {
        std::optional<kiriview::PreparedImageDecodeResult> terminal
            = executePreparedWork(std::move(*rasterWork), workspaceBudget);
        QVERIFY(terminal.has_value());
        const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
        QVERIFY(result != nullptr);
        const auto* decoded
            = kiriview::decodedImageResultImageAs<kiriview::StaticDecodedImage>(*result);
        QVERIFY(decoded != nullptr);
        QCOMPARE(decoded->displayImage.image.size(), sourceImage.size());
        QCOMPARE(
            decoded->displayImage.inputWorkspaceHold.reservedByteCount(), retainedInputByteCount);
        QCOMPARE(sourceBudget->reservedByteCount(), originalData.size());
    }
    QCOMPARE(workspaceBudget->reservedByteCount(), qsizetype(0));
    QCOMPARE(sourceBudget->reservedByteCount(), qsizetype(0));
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

void TestImageDecodePipeline::preparedQtRasterStillDeclaresExactProducerEnvelope()
{
    QImage sourceImage(QSize(6, 4), QImage::Format_RGBA8888);
    sourceImage.fill(Qt::blue);
    QString errorString;
    const QByteArray data = encodedImageData(sourceImage, QByteArrayLiteral("png"), &errorString);
    QVERIFY2(!data.isEmpty(), qPrintable(errorString));

    kiriview::ImageDecodeRouter router({}, [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Png);
    });
    constexpr qsizetype workspaceLimit = 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(
        9, QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-static.png")));

    kiriview::PreparedImageDecodeResult plan = router.prepare(kiriview::ImageSourceData(data),
        request, kiriview::ImageDecodeWorkspacePriority::Interactive, budget);
    auto* work = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&plan);
    QVERIFY(work != nullptr);
    QVERIFY(*work != nullptr);
    const kiriview::ImageDecodeWorkspaceAdmissionRequest admission
        = work->get()->admissionRequest();
    QVERIFY(admission.additionalPeakByteCount > 0);
    QVERIFY(admission.additionalPeakByteCount < workspaceLimit);
    QCOMPARE(admission.perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(admission.priority, kiriview::ImageDecodeWorkspacePriority::Interactive);
    QCOMPARE(work->get()->hardLimitFailure().route, kiriview::DecodedImageFailureRoute::QtRaster);
    QCOMPARE(work->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage);
    QCOMPARE(work->get()->hardLimitFailure().cause,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));

    std::optional<kiriview::PreparedImageDecodeResult> terminal
        = executePreparedWork(std::move(*work), budget);
    QVERIFY(terminal.has_value());
    const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
    QVERIFY(result != nullptr);
    const auto* decoded
        = kiriview::decodedImageResultImageAs<kiriview::StaticDecodedImage>(*result);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->displayImage.image.size(), sourceImage.size());
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

void TestImageDecodePipeline::preparedSvgSeparatesParserAndRasterEnvelopes()
{
    const QByteArray data
        = QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"80\" height=\"40\">"
                            "<rect width=\"80\" height=\"40\" fill=\"red\"/>"
                            "</svg>");
    kiriview::ImageDecodeRouter router({}, [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::Svg);
    });
    constexpr qsizetype workspaceLimit = qsizetype { 768 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(10,
        QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-vector.svg")),
        kiriview::ImageFirstDisplayDecodeContext { QSize(200, 200) });

    kiriview::PreparedImageDecodeResult parserPlan = router.prepare(kiriview::ImageSourceData(data),
        request, kiriview::ImageDecodeWorkspacePriority::Interactive, budget);
    auto* parserWork = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&parserPlan);
    QVERIFY(parserWork != nullptr);
    QVERIFY(*parserWork != nullptr);
    QCOMPARE(parserWork->get()->admissionRequest().additionalPeakByteCount,
        *kiriview::svgParserWorkspaceByteCost(data.size()));
    QCOMPARE(parserWork->get()->hardLimitFailure().route, kiriview::DecodedImageFailureRoute::Svg);
    QCOMPARE(parserWork->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::OpenStaticImageSource);
    const qsizetype parserPeakByteCount
        = parserWork->get()->admissionRequest().additionalPeakByteCount;

    std::optional<kiriview::PreparedImageDecodeResult> rasterPlan
        = executePreparedWork(std::move(*parserWork), budget);
    QVERIFY(rasterPlan.has_value());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
    auto* rasterWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*rasterPlan);
    QVERIFY(rasterWork != nullptr);
    QVERIFY(*rasterWork != nullptr);
    QVERIFY(rasterWork->get()->admissionRequest().additionalPeakByteCount > parserPeakByteCount);
    QVERIFY(rasterWork->get()->admissionRequest().additionalPeakByteCount < workspaceLimit);
    QCOMPARE(rasterWork->get()->admissionRequest().perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(rasterWork->get()->hardLimitFailure().route, kiriview::DecodedImageFailureRoute::Svg);
    QCOMPARE(rasterWork->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeBlockingDisplayImage);

    std::optional<kiriview::PreparedImageDecodeResult> terminal
        = executePreparedWork(std::move(*rasterWork), budget);
    QVERIFY(terminal.has_value());
    const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
    QVERIFY(result != nullptr);
    const auto* decoded
        = kiriview::decodedImageResultImageAs<kiriview::StaticDecodedImage>(*result);
    QVERIFY(decoded != nullptr);
    QCOMPARE(decoded->displayImage.image.size(), QSize(200, 100));
}

void TestImageDecodePipeline::preparedRawCarriesOpenHoldIntoExactProduction()
{
    const QByteArray data = fixtureData(QStringLiteral("raw-cfa-smoke.dng"));
    QVERIFY(!data.isEmpty());
    kiriview::ImageDecodeRouter router({}, [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::Raw);
    });
    constexpr qsizetype workspaceLimit = qsizetype { 1024 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(
        13, QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-raw.dng")));

    kiriview::PreparedImageDecodeResult openPlan = router.prepare(kiriview::ImageSourceData(data),
        request, kiriview::ImageDecodeWorkspacePriority::Demanded, budget);
    auto* openWork = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&openPlan);
    QVERIFY(openWork != nullptr);
    QVERIFY(*openWork != nullptr);
    QCOMPARE(openWork->get()->admissionRequest().additionalPeakByteCount,
        kiriview::rawImageOpenWorkspaceByteCount);
    QCOMPARE(openWork->get()->admissionRequest().perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(openWork->get()->admissionRequest().priority,
        kiriview::ImageDecodeWorkspacePriority::Demanded);
    QCOMPARE(openWork->get()->hardLimitFailure().route, kiriview::DecodedImageFailureRoute::Raw);
    QCOMPARE(openWork->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeRawImage);

    std::optional<kiriview::PreparedImageDecodeResult> productionPlan
        = executePreparedWork(std::move(*openWork), budget);
    QVERIFY(productionPlan.has_value());
    QCOMPARE(budget->reservedByteCount(), kiriview::rawImageOpenWorkspaceByteCount);
    auto* productionWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*productionPlan);
    QVERIFY(productionWork != nullptr);
    QVERIFY(*productionWork != nullptr);
    QCOMPARE(productionWork->get()->admissionRequest().perOperationBaselineByteCount,
        kiriview::rawImageOpenWorkspaceByteCount);
    QVERIFY(productionWork->get()->admissionRequest().additionalPeakByteCount > 0);
    QVERIFY(productionWork->get()->admissionRequest().additionalPeakByteCount
        < workspaceLimit - kiriview::rawImageOpenWorkspaceByteCount);
    QCOMPARE(productionWork->get()->admissionRequest().priority,
        kiriview::ImageDecodeWorkspacePriority::Demanded);

    {
        std::optional<kiriview::PreparedImageDecodeResult> terminal
            = executePreparedWork(std::move(*productionWork), budget);
        QVERIFY(terminal.has_value());
        const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
        QVERIFY(result != nullptr);
        const auto* decoded
            = kiriview::decodedImageResultImageAs<kiriview::StaticDecodedImage>(*result);
        const auto* failure = kiriview::decodedImageResultFailure(*result);
        QVERIFY2(decoded != nullptr,
            qPrintable(failure != nullptr ? failure->diagnosticDetail
                                          : QStringLiteral("RAW fixture did not decode.")));
        QCOMPARE(decoded->displayImage.originalSize, QSize(32, 32));
        QVERIFY(budget->reservedByteCount() > 0);
        QVERIFY(budget->reservedByteCount() < kiriview::rawImageOpenWorkspaceByteCount);
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::preparedRawCompatibleInputBaselineIncludesLiveOpenHold()
{
    const QByteArray data = fixtureData(QStringLiteral("raw-cfa-smoke.dng"));
    QVERIFY(!data.isEmpty());
    qsizetype retainedInputByteCount = 0;
    kiriview::ImageDecodeRouter router(
        {},
        [](const QByteArray&, const QString&) {
            return classification(kiriview::ImageInputKind::Raw, kiriview::QtRasterFormat::None,
                kiriview::ImageDecodeDataSource::AvifCompatible);
        },
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) { return std::optional(sourceByteCount + 4096); },
            [&retainedInputByteCount](const QByteArray& source) {
                QByteArray replacement(source.constData(), source.size());
                replacement.reserve(source.size() + 1024);
                retainedInputByteCount = replacement.capacity();
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    std::move(replacement),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
        });
    constexpr qsizetype workspaceLimit = qsizetype { 1024 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);

    kiriview::PreparedImageDecodeResult transformPlan
        = router.prepare(kiriview::ImageSourceData(data), kiriview::ImageDecodeRequest {},
            kiriview::ImageDecodeWorkspacePriority::Speculative, budget);
    auto* transformWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&transformPlan);
    QVERIFY(transformWork != nullptr);
    QVERIFY(*transformWork != nullptr);
    std::optional<kiriview::PreparedImageDecodeResult> openPlan
        = executePreparedWork(std::move(*transformWork), budget);
    QVERIFY(openPlan.has_value());
    QVERIFY(retainedInputByteCount > data.size());
    QCOMPARE(budget->reservedByteCount(), retainedInputByteCount);

    auto* openWork = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*openPlan);
    QVERIFY(openWork != nullptr);
    QVERIFY(*openWork != nullptr);
    QCOMPARE(openWork->get()->admissionRequest().additionalPeakByteCount,
        kiriview::rawImageOpenWorkspaceByteCount);
    QCOMPARE(
        openWork->get()->admissionRequest().perOperationBaselineByteCount, retainedInputByteCount);
    QCOMPARE(openWork->get()->admissionRequest().priority,
        kiriview::ImageDecodeWorkspacePriority::Speculative);

    std::optional<kiriview::PreparedImageDecodeResult> productionPlan
        = executePreparedWork(std::move(*openWork), budget);
    QVERIFY(productionPlan.has_value());
    QCOMPARE(budget->reservedByteCount(),
        retainedInputByteCount + kiriview::rawImageOpenWorkspaceByteCount);
    auto* productionWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*productionPlan);
    QVERIFY(productionWork != nullptr);
    QVERIFY(*productionWork != nullptr);
    QCOMPARE(productionWork->get()->admissionRequest().perOperationBaselineByteCount,
        retainedInputByteCount + kiriview::rawImageOpenWorkspaceByteCount);

    productionPlan.reset();
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::preparedHeifSequenceSeparatesProbeAndProduction()
{
    const QByteArray data = fixtureData(QStringLiteral("heif-sequence-alpha.heics"));
    QVERIFY(!data.isEmpty());
    kiriview::ImageDecodeRouter router({}, [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::HeifFamily);
    });
    constexpr qsizetype workspaceLimit = qsizetype { 256 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(
        14, QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-sequence.heics")));

    kiriview::PreparedImageDecodeResult probePlan = router.prepare(kiriview::ImageSourceData(data),
        request, kiriview::ImageDecodeWorkspacePriority::Interactive, budget);
    auto* probeWork = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&probePlan);
    QVERIFY(probeWork != nullptr);
    QVERIFY(*probeWork != nullptr);
    QCOMPARE(probeWork->get()->admissionRequest().additionalPeakByteCount,
        kiriview::heifSequenceProbeWorkspaceByteCount);
    QCOMPARE(probeWork->get()->admissionRequest().perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(
        probeWork->get()->hardLimitFailure().route, kiriview::DecodedImageFailureRoute::HeifFamily);
    QCOMPARE(probeWork->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeHeifSequenceOpen);

    std::optional<kiriview::PreparedImageDecodeResult> productionPlan
        = executePreparedWork(std::move(*probeWork), budget);
    QVERIFY(productionPlan.has_value());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
    auto* productionWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*productionPlan);
    QVERIFY(productionWork != nullptr);
    QVERIFY(*productionWork != nullptr);
    const kiriview::HeifSequenceWorkspacePlanResult expected
        = kiriview::planHeifSequenceOpen(data, budget);
    QCOMPARE(expected.status, kiriview::HeifSequenceOpenStatus::Success);
    QCOMPARE(productionWork->get()->admissionRequest().additionalPeakByteCount,
        expected.plan.transientByteCount + (2 * expected.plan.outputByteCount));
    QCOMPARE(productionWork->get()->admissionRequest().perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(productionWork->get()->admissionRequest().priority,
        kiriview::ImageDecodeWorkspacePriority::Interactive);
    QCOMPARE(productionWork->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeHeifSequenceFrame);

    {
        std::optional<kiriview::PreparedImageDecodeResult> terminal
            = executePreparedWork(std::move(*productionWork), budget);
        QVERIFY(terminal.has_value());
        const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
        QVERIFY(result != nullptr);
        const auto* decoded
            = kiriview::decodedImageResultImageAs<kiriview::HeifSequenceAnimationImage>(*result);
        const auto* failure = kiriview::decodedImageResultFailure(*result);
        QVERIFY2(decoded != nullptr,
            qPrintable(failure != nullptr ? failure->diagnosticDetail
                                          : QStringLiteral("HEIF sequence did not decode.")));
        QCOMPARE(decoded->firstFrame.size(), QSize(64, 64));
        QVERIFY(decoded->firstFrameWorkspaceHold.isManaged());
        QCOMPARE(budget->reservedByteCount(), decoded->firstFrameWorkspaceHold.reservedByteCount());
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::preparedRawAndHeifHardLimitsPreserveTypedRoutes_data()
{
    QTest::addColumn<int>("inputKind");
    QTest::addColumn<QString>("fixtureName");
    QTest::addColumn<qsizetype>("requiredOpenByteCount");
    QTest::addColumn<int>("expectedRoute");
    QTest::addColumn<int>("expectedOperation");

    QTest::newRow("raw") << static_cast<int>(kiriview::ImageInputKind::Raw)
                         << QStringLiteral("raw-cfa-smoke.dng")
                         << kiriview::rawImageOpenWorkspaceByteCount
                         << static_cast<int>(kiriview::DecodedImageFailureRoute::Raw)
                         << static_cast<int>(
                                kiriview::DecodedImageFailureOperation::DecodeRawImage);
    QTest::newRow("heif-sequence")
        << static_cast<int>(kiriview::ImageInputKind::HeifFamily)
        << QStringLiteral("heif-sequence-alpha.heics")
        << kiriview::heifSequenceProbeWorkspaceByteCount
        << static_cast<int>(kiriview::DecodedImageFailureRoute::HeifFamily)
        << static_cast<int>(kiriview::DecodedImageFailureOperation::DecodeHeifSequenceOpen);
}

void TestImageDecodePipeline::preparedRawAndHeifHardLimitsPreserveTypedRoutes()
{
    QFETCH(int, inputKind);
    QFETCH(QString, fixtureName);
    QFETCH(qsizetype, requiredOpenByteCount);
    QFETCH(int, expectedRoute);
    QFETCH(int, expectedOperation);

    const QByteArray data = fixtureData(fixtureName);
    QVERIFY(!data.isEmpty());
    const auto kind = static_cast<kiriview::ImageInputKind>(inputKind);
    kiriview::ImageDecodeRouter router(
        {}, [kind](const QByteArray&, const QString&) { return classification(kind); });
    auto budget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        requiredOpenByteCount - 1, requiredOpenByteCount - 1);

    kiriview::PreparedImageDecodeResult plan
        = router.prepare(kiriview::ImageSourceData(data), kiriview::ImageDecodeRequest {},
            kiriview::ImageDecodeWorkspacePriority::Interactive, budget);
    auto* work = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&plan);
    QVERIFY(work != nullptr);
    QVERIFY(*work != nullptr);
    QCOMPARE(work->get()->admissionRequest().additionalPeakByteCount, requiredOpenByteCount);
    QVERIFY(
        work->get()->admissionRequest().additionalPeakByteCount > budget->perOperationByteLimit());
    QCOMPARE(static_cast<int>(work->get()->hardLimitFailure().route), expectedRoute);
    QCOMPARE(static_cast<int>(work->get()->hardLimitFailure().operation), expectedOperation);
    QCOMPARE(work->get()->hardLimitFailure().cause,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QVERIFY(!work->get()->hardLimitFailure().retryable);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodePipeline::preparedDefaultAnimationDeclaresBoundedEnvelope_data()
{
    QTest::addColumn<int>("inputKind");
    QTest::addColumn<int>("qtFormat");
    QTest::addColumn<QString>("fixtureName");

    QTest::newRow("apng") << static_cast<int>(kiriview::ImageInputKind::Apng)
                          << static_cast<int>(kiriview::QtRasterFormat::None)
                          << QStringLiteral("animated-smoke.apng");
    QTest::newRow("gif") << static_cast<int>(kiriview::ImageInputKind::QtRaster)
                         << static_cast<int>(kiriview::QtRasterFormat::Gif) << QString();
    QTest::newRow("webp") << static_cast<int>(kiriview::ImageInputKind::QtRaster)
                          << static_cast<int>(kiriview::QtRasterFormat::Webp)
                          << QStringLiteral("animated-smoke.webp");
}

void TestImageDecodePipeline::preparedDefaultAnimationDeclaresBoundedEnvelope()
{
    QFETCH(int, inputKind);
    QFETCH(int, qtFormat);
    QFETCH(QString, fixtureName);

    const QByteArray data = fixtureName.isEmpty() ? animatedGifData() : fixtureData(fixtureName);
    QVERIFY2(!data.isEmpty(), qPrintable(fixtureName));
    const auto kind = static_cast<kiriview::ImageInputKind>(inputKind);
    const auto format = static_cast<kiriview::QtRasterFormat>(qtFormat);
    kiriview::ImageDecodeRouter router({},
        [kind, format](const QByteArray&, const QString&) { return classification(kind, format); });
    constexpr qsizetype workspaceLimit = qsizetype { 256 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(
        12, QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-animation")));

    kiriview::PreparedImageDecodeResult plan = router.prepare(kiriview::ImageSourceData(data),
        request, kiriview::ImageDecodeWorkspacePriority::Demanded, budget);
    auto* work = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&plan);
    QVERIFY(work != nullptr);
    QVERIFY(*work != nullptr);
    const kiriview::ImageDecodeWorkspaceAdmissionRequest admission
        = work->get()->admissionRequest();
    QVERIFY(admission.additionalPeakByteCount > 0);
    QVERIFY(admission.additionalPeakByteCount < workspaceLimit);
    QCOMPARE(admission.perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(admission.priority, kiriview::ImageDecodeWorkspacePriority::Demanded);
    QCOMPARE(work->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeAnimationOpen);

    std::optional<kiriview::PreparedImageDecodeResult> terminal
        = executePreparedWork(std::move(*work), budget);
    QVERIFY(terminal.has_value());
    const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
    QVERIFY(result != nullptr);
    QVERIFY(kiriview::decodedImageResultImage(*result) != nullptr);
}

void TestImageDecodePipeline::preparedJxlAnimationUsesPrechargedAllocatorEnvelope()
{
    const QByteArray data = fixtureData(QStringLiteral("animated-smoke.jxl"));
    QVERIFY(!data.isEmpty());
    kiriview::ImageDecodeRouter router({}, [](const QByteArray&, const QString&) {
        return classification(kiriview::ImageInputKind::QtRaster, kiriview::QtRasterFormat::Jxl);
    });
    constexpr qsizetype workspaceLimit = qsizetype { 1024 } * 1024 * 1024;
    auto budget
        = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(workspaceLimit, workspaceLimit);
    const kiriview::ImageDecodeRequest request = kiriview::ImageDecodeRequest::fromUrl(
        11, QUrl::fromLocalFile(QStringLiteral("/tmp/prepared-animation.jxl")));

    kiriview::PreparedImageDecodeResult catalogPlan
        = router.prepare(kiriview::ImageSourceData(data), request,
            kiriview::ImageDecodeWorkspacePriority::Interactive, budget);
    auto* catalogWork
        = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&catalogPlan);
    QVERIFY(catalogWork != nullptr);
    QVERIFY(*catalogWork != nullptr);
    QCOMPARE(catalogWork->get()->admissionRequest().additionalPeakByteCount,
        kiriview::jxlAnimationDecoderAllocationByteLimit);
    QCOMPARE(catalogWork->get()->admissionRequest().perOperationBaselineByteCount, qsizetype(0));
    QCOMPARE(catalogWork->get()->hardLimitFailure().operation,
        kiriview::DecodedImageFailureOperation::DecodeAnimationOpen);

    std::optional<kiriview::PreparedImageDecodeResult> openPlan
        = executePreparedWork(std::move(*catalogWork), budget);
    QVERIFY(openPlan.has_value());
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
    auto* openWork = std::get_if<std::unique_ptr<kiriview::PreparedImageDecodeWork>>(&*openPlan);
    QVERIFY(openWork != nullptr);
    QVERIFY(*openWork != nullptr);
    const std::optional<qsizetype> expectedPeak
        = kiriview::jxlAnimationOpenWorkspaceByteCount(QSize(2, 1));
    QVERIFY(expectedPeak.has_value());
    QCOMPARE(openWork->get()->admissionRequest().additionalPeakByteCount, *expectedPeak);
    QVERIFY(*expectedPeak > kiriview::jxlAnimationDecoderAllocationByteLimit);
    QVERIFY(*expectedPeak < workspaceLimit);

    {
        std::optional<kiriview::PreparedImageDecodeResult> terminal
            = executePreparedWork(std::move(*openWork), budget);
        QVERIFY(terminal.has_value());
        const auto* result = std::get_if<kiriview::DecodedImageResult>(&*terminal);
        QVERIFY(result != nullptr);
        const auto* decoded
            = kiriview::decodedImageResultImageAs<kiriview::JxlAnimationImage>(*result);
        QVERIFY(decoded != nullptr);
        QCOMPARE(decoded->firstFrame.size(), QSize(2, 1));
        QCOMPARE(decoded->firstFrameWorkspaceHold.reservedByteCount(), qsizetype(8));
        QCOMPARE(budget->reservedByteCount(), qsizetype(8));
    }
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
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
        kiriview::ImageDecodeCompatibleDataTransform {
            [](qsizetype sourceByteCount) {
                constexpr qsizetype suffixSize = 11;
                return sourceByteCount <= std::numeric_limits<qsizetype>::max() - suffixSize
                    ? std::optional(sourceByteCount + suffixSize)
                    : std::nullopt;
            },
            [&compatibleTransformCount](const QByteArray& data) {
                ++compatibleTransformCount;
                return kiriview::ImageDecodeCompatibleDataTransform::Result {
                    data + QByteArrayLiteral("-compatible"),
                    kiriview::ImageDecodeCompatibleDataTransform::Storage::OwnedReplacement,
                };
            },
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
