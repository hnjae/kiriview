// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodejob.h"
#include "decoding/imagesourcedata.h"
#include "decoding/rawdecoder.h"
#include "decoding/rawthumbnailpreview.h"
#include "image_test_support.h"
#include "system/kiooperationfailure.h"
#include "thumbnail/thumbnailcachelookup.h"

#include <KIO/Global>
#include <QBuffer>
#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSemaphore>
#include <QSize>
#include <QTest>
#include <QThreadPool>
#include <QUrl>
#include <atomic>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace {
using kiriview::TestSupport::imageDecodeDependenciesFor;
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::ManualImageDataLoader;
using kiriview::TestSupport::staticImageDataDecoderRejectingBadData;
using kiriview::TestSupport::testImageDecodeFailureString;

kiriview::ImageDecodeJob::Callbacks decodeJobCallbacks(
    kiriview::ImageDecodeJob::DecodedCallback decoded = {},
    kiriview::ImageDecodeJob::LoadErrorCallback loadError = {},
    kiriview::ImageDecodeJob::ThumbnailPreviewCallback thumbnailPreview = {},
    kiriview::ImageDecodeJob::RetiredCallback retired = {})
{
    return kiriview::ImageDecodeJob::Callbacks {
        std::move(decoded),
        std::move(loadError),
        std::move(thumbnailPreview),
        std::move(retired),
    };
}

QByteArray encodedPngData(const QSize& size)
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(QColor(Qt::green));

    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, QByteArrayLiteral("png"));
    if (!writer.write(image)) {
        return {};
    }
    return data;
}

QImage thumbnailImage(const QSize& size = QSize(400, 300))
{
    QImage image(size, QImage::Format_RGBA8888);
    image.fill(QColor(Qt::blue));
    return image;
}

kiriview::ThumbnailCacheLookupResult readyThumbnailLookup(QImage image = thumbnailImage())
{
    return kiriview::ThumbnailCacheLookupResult {
        kiriview::ThumbnailCacheLookupStatus::Ready,
        std::move(image),
        kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge,
        kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge,
        QStringLiteral("/cache/photo.png"),
        {},
    };
}

kiriview::ThumbnailCacheLookupResult missingThumbnailLookup()
{
    return kiriview::ThumbnailCacheLookupResult {
        kiriview::ThumbnailCacheLookupStatus::Missing,
        {},
        kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge,
        kiriview::ActiveNavigationThumbnailDemandBucket::None,
        {},
        {},
    };
}

kiriview::DecodedImageFailure preparedHardLimitTestFailure()
{
    return kiriview::DecodedImageFailure {
        QStringLiteral("workspace rejected"),
        kiriview::DecodedImageFailureRoute::Svg,
        kiriview::DecodedImageFailureOperation::DecodeFirstDisplayImage,
        QStringLiteral("prepared SVG raster exceeds its hard envelope"),
        kiriview::DecodedImageFailureSeverity::Error,
        false,
        kiriview::DecodedImageFailureCause::ResourceLimitExceeded,
    };
}

QByteArray rawFixtureData()
{
    QFile file(QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/raw-cfa-smoke.dng"));
    if (!file.open(QFile::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

class ManualThumbnailLookupProvider
{
public:
    kiriview::ThumbnailCacheLookupProvider provider()
    {
        return [this](QObject*, kiriview::ThumbnailCacheLookupRequest request,
                   kiriview::ThumbnailCacheLookupCallback callback) {
            requests.push_back(std::move(request));
            callbacks.push_back(std::move(callback));
            return kiriview::ImageIoJob();
        };
    }

    void finish(std::size_t index, kiriview::ThumbnailCacheLookupResult result)
    {
        if (index >= callbacks.size() || !callbacks.at(index)) {
            return;
        }

        kiriview::ThumbnailCacheLookupCallback callback = std::move(callbacks.at(index));
        callback(std::move(result));
    }

    std::vector<kiriview::ThumbnailCacheLookupRequest> requests;
    std::vector<kiriview::ThumbnailCacheLookupCallback> callbacks;
};

struct ManualImageWorkerSchedule
{
    kiriview::ImageWorkerOperation work;
    kiriview::ImageWorkerCompletion completion;
    kiriview::ImageWorkerTaskCompletion taskCompletion;
    bool workStarted = false;
};

class ManualImageWorkerScheduler
{
public:
    kiriview::ImageWorkerScheduler scheduler()
    {
        return kiriview::ImageWorkerScheduler([this](QObject*, kiriview::ImageWorkerOperation work,
                                                  kiriview::ImageWorkerCompletion completion) {
            auto schedule = std::make_shared<ManualImageWorkerSchedule>();
            schedule->work = std::move(work);
            schedule->completion = std::move(completion);
            kiriview::ImageWorkerTask task(
                [weakSchedule = std::weak_ptr<ManualImageWorkerSchedule>(schedule)]() {
                    if (const auto activeSchedule = weakSchedule.lock()) {
                        activeSchedule->work = {};
                        activeSchedule->completion = {};
                        if (!activeSchedule->workStarted) {
                            activeSchedule->taskCompletion.retire();
                        }
                    }
                });
            schedule->taskCompletion = task.completion();
            m_schedules.push_back(std::move(schedule));
            return task;
        });
    }

    std::size_t scheduleCount() const { return m_schedules.size(); }

    void runWork(std::size_t index)
    {
        m_schedules.at(index)->workStarted = true;
        if (m_schedules.at(index)->work) {
            m_schedules.at(index)->work();
        }
    }

    void finish(std::size_t index)
    {
        const auto& schedule = m_schedules.at(index);
        schedule->taskCompletion.claimAndRun([&]() {
            if (schedule->completion) {
                schedule->completion();
            }
        });
        schedule->work = {};
        schedule->completion = {};
        schedule->taskCompletion.retire();
    }

private:
    std::vector<std::shared_ptr<ManualImageWorkerSchedule>> m_schedules;
};

class SemaphoreReleaseOnExit
{
public:
    explicit SemaphoreReleaseOnExit(QSemaphore& semaphore)
        : m_semaphore(&semaphore)
    {
    }

    ~SemaphoreReleaseOnExit()
    {
        if (m_semaphore != nullptr) {
            m_semaphore->release();
        }
    }

    void dismiss() { m_semaphore = nullptr; }

private:
    QSemaphore* m_semaphore = nullptr;
};

class MinimumThreadPoolConcurrency
{
public:
    explicit MinimumThreadPoolConcurrency(int minimumThreadCount)
        : m_previousThreadCount(QThreadPool::globalInstance()->maxThreadCount())
    {
        if (m_previousThreadCount < minimumThreadCount) {
            QThreadPool::globalInstance()->setMaxThreadCount(minimumThreadCount);
        }
    }

    ~MinimumThreadPoolConcurrency()
    {
        QThreadPool::globalInstance()->setMaxThreadCount(m_previousThreadCount);
    }

private:
    int m_previousThreadCount = 0;
};

class ManualRawEmbeddedThumbnailPreviewExtractor
{
public:
    kiriview::RawEmbeddedThumbnailPreviewExtractor extractor()
    {
        return [this](const QByteArray& data, const kiriview::ImageDecodeRequest& request,
                   kiriview::ImageDecodeWorkspaceLease workspaceLease) {
            if (started != nullptr) {
                started->release();
            }
            if (mayReturn != nullptr) {
                mayReturn->acquire();
            }

            std::lock_guard lock(m_mutex);
            ++m_callCount;
            m_requests.push_back(request);
            m_dataSizes.push_back(data.size());
            m_workspaceByteCounts.push_back(workspaceLease.reservedByteCount());
            return result;
        };
    }

    int callCount() const
    {
        std::lock_guard lock(m_mutex);
        return m_callCount;
    }

    std::vector<kiriview::ImageDecodeRequest> requests() const
    {
        std::lock_guard lock(m_mutex);
        return m_requests;
    }

    std::vector<qsizetype> dataSizes() const
    {
        std::lock_guard lock(m_mutex);
        return m_dataSizes;
    }

    std::vector<qsizetype> workspaceByteCounts() const
    {
        std::lock_guard lock(m_mutex);
        return m_workspaceByteCounts;
    }

    kiriview::RawEmbeddedThumbnailPreviewResult result {
        kiriview::RawEmbeddedThumbnailPreviewStatus::Ready,
        thumbnailImage(QSize(16, 16)),
        QSize(32, 32),
        {},
    };
    QSemaphore* started = nullptr;
    QSemaphore* mayReturn = nullptr;

private:
    mutable std::mutex m_mutex;
    int m_callCount = 0;
    std::vector<kiriview::ImageDecodeRequest> m_requests;
    std::vector<qsizetype> m_dataSizes;
    std::vector<qsizetype> m_workspaceByteCounts;
};

kiriview::ImageDecodeDependencies imageDecodeDependenciesForThumbnailPreview(
    ManualImageDataLoader& dataLoader, kiriview::ImageDataDecoder dataDecoder,
    ManualThumbnailLookupProvider& thumbnailLookup,
    ManualRawEmbeddedThumbnailPreviewExtractor* rawExtractor = nullptr)
{
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, std::move(dataDecoder));
    dependencies.thumbnailPreviewLookupProvider = thumbnailLookup.provider();
    if (rawExtractor != nullptr) {
        dependencies.rawEmbeddedThumbnailPreviewExtractor = rawExtractor->extractor();
    }
    return dependencies;
}
}

class TestImageDecodeJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void emptyRequestLeavesDiagnosticWarning();
    void cancelSuppressesPendingLoad();
    void staleLoadResultIsIgnored();
    void staleTypedLoadErrorIsIgnored();
    void restartedSameRequestIgnoresStaleLoadResult();
    void loadErrorsAreDeliveredForCurrentRequest();
    void unmanagedLoadedDataOverBudgetReturnsTypedResourceFailure();
    void decodeErrorsAreDeliveredAsResults();
    void decodeRequestIsPassedToDecoder();
    void decodeWorkerSchedulerCanBeDrivenManually();
    void temporaryWorkspaceContentionWaitsWithoutFailure();
    void intrinsicWorkspaceRejectionPreservesTypedFailure();
    void cancelingPendingAdmissionSuppressesProducerAndPublication();
    void retirementWaitsForRunningProducerAndFiresOnce();
    void synchronousDependenciesRetireExactlyOnceAfterPublication();
    void synchronousPlannerCompletionAllowsCallbackToDestroyJob();
    void synchronousPreviewCompletionAllowsCallbackToDestroyJob();
    void retainedDecodedPayloadRetainsSourceDataReservation();
    void sourceReplacementCancelsQueuedDecodeWork();
    void xdgThumbnailPreviewIsDeliveredBeforeDecodeCompletes();
    void staleXdgThumbnailPreviewIsIgnored();
    void nonRawXdgMissDoesNotRunRawEmbeddedPreview();
    void rawEmbeddedPreviewIsDeliveredAfterXdgMiss();
    void rawEmbeddedPreviewContentionWaitsBeforeWorker();
    void cancelingPendingRawEmbeddedPreviewSuppressesWorker();
    void terminalDecodeCancelsPendingPreviewAdmission();
    void intrinsicPreviewLimitSkipsWithoutFailingDecode();
    void canceledRunningRawPreviewRetainsSourceUntilPhysicalRetirement();
    void rawEmbeddedPreviewMissDoesNotPublish();
    void lateRawEmbeddedPreviewAfterDecodeIsIgnored();
    void readyXdgThumbnailPreviewSuppressesRawEmbeddedPreview();
};

void TestImageDecodeJob::emptyRequestLeavesDiagnosticWarning()
{
    ManualImageDataLoader dataLoader;
    int decodedCount = 0;
    int loadErrorCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData()),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            [&loadErrorCount](const kiriview::ImageDecodeRequest&,
                const kiriview::ImageDataLoadError&) { ++loadErrorCount; }));

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(".*KiriView image decode rejected empty request.*"));
    decodeJob.start(kiriview::ImageDecodeRequest());

    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
    QCOMPARE(decodedCount, 0);
    QCOMPARE(loadErrorCount, 0);
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::cancelSuppressesPendingLoad()
{
    ManualImageDataLoader dataLoader;
    int decodedCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData()),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(1, indexedImageUrl(1)));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    decodeJob.cancel();

    QVERIFY(dataLoader.frontLoad().canceled);
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));

    QCOMPARE(decodedCount, 0);
}

void TestImageDecodeJob::staleLoadResultIsIgnored()
{
    ManualImageDataLoader dataLoader;
    std::vector<kiriview::ImageDecodeRequest> decodedRequests;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData()),
        decodeJobCallbacks(
            [&decodedRequests](kiriview::ImageDecodeRequest request, kiriview::DecodedImageResult) {
                decodedRequests.push_back(request);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(1, indexedImageUrl(1)));
    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(2, indexedImageUrl(2)));
    QCOMPARE(dataLoader.loadCount(), std::size_t(2));
    QVERIFY(dataLoader.frontLoad().canceled);

    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("ok"));
    dataLoader.finishBackLoad(QByteArrayLiteral("ok"));

    QTRY_COMPARE(decodedRequests.size(), std::size_t(1));
    QCOMPARE(decodedRequests.front().id(), quint64(2));
    QCOMPARE(decodedRequests.front().imageUrl(), indexedImageUrl(2));
}

void TestImageDecodeJob::staleTypedLoadErrorIsIgnored()
{
    ManualImageDataLoader dataLoader;
    std::vector<kiriview::ImageDecodeRequest> errorRequests;
    std::vector<kiriview::ImageDataLoadError> deliveredErrors;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData()),
        decodeJobCallbacks({},
            [&errorRequests, &deliveredErrors](const kiriview::ImageDecodeRequest& request,
                const kiriview::ImageDataLoadError& error) {
                errorRequests.push_back(request);
                deliveredErrors.push_back(error);
            }));
    const QUrl staleUrl = indexedImageUrl(1);
    const QUrl currentUrl = indexedImageUrl(2);

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(1, staleUrl));
    const kiriview::ImageDataLoadErrorCallback staleErrorCallback
        = dataLoader.frontLoad().errorCallback;
    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(2, currentUrl));
    QVERIFY(dataLoader.frontLoad().canceled);

    staleErrorCallback(kiriview::ImageDataLoadError {
        kiriview::kioOperationFailureFromKJob(kiriview::KioOperationKind::ImageDataRead, staleUrl,
            KIO::ERR_CONNECTION_BROKEN, QStringLiteral("stale backend failure")) });
    QCOMPARE(errorRequests.size(), std::size_t(0));
    QVERIFY(decodeJob.hasActiveRequest());

    const kiriview::KioOperationFailure expectedFailure
        = kiriview::kioOperationResourceLimitFailure(kiriview::KioOperationKind::ImageDataRead,
            currentUrl, QStringLiteral("current resource failure"));
    dataLoader.failBackLoad(kiriview::ImageDataLoadError { expectedFailure });

    QCOMPARE(errorRequests.size(), std::size_t(1));
    QCOMPARE(errorRequests.front().id(), quint64(2));
    QCOMPARE(errorRequests.front().imageUrl(), currentUrl);
    QCOMPARE(deliveredErrors.size(), std::size_t(1));
    const auto* failure = std::get_if<kiriview::KioOperationFailure>(&deliveredErrors.front());
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->operationKind, expectedFailure.operationKind);
    QCOMPARE(failure->targetUrl, expectedFailure.targetUrl);
    QCOMPARE(failure->cause, expectedFailure.cause);
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::restartedSameRequestIgnoresStaleLoadResult()
{
    ManualImageDataLoader dataLoader;
    QByteArray decodedData;
    int decodedCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader,
            [&decodedData](const QByteArray& data, const kiriview::ImageDecodeRequest&) {
                decodedData = data;
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage());
            }),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; }));
    const kiriview::ImageDecodeRequest request
        = kiriview::ImageDecodeRequest::fromUrl(7, indexedImageUrl(7));

    decodeJob.start(request);
    decodeJob.start(request);
    QCOMPARE(dataLoader.loadCount(), std::size_t(2));
    QVERIFY(dataLoader.frontLoad().canceled);

    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("old"));
    QCOMPARE(decodedCount, 0);

    dataLoader.finishBackLoad(QByteArrayLiteral("new"));

    QTRY_COMPARE(decodedCount, 1);
    QCOMPARE(decodedData, QByteArrayLiteral("new"));
}

void TestImageDecodeJob::loadErrorsAreDeliveredForCurrentRequest()
{
    ManualImageDataLoader dataLoader;
    std::vector<kiriview::ImageDecodeRequest> errorRequests;
    std::optional<kiriview::ImageDataLoadError> deliveredError;
    const QUrl imageUrl = indexedImageUrl(3);
    const kiriview::KioOperationFailure expectedFailure {
        kiriview::KioOperationKind::ImageDataRead,
        imageUrl,
        73,
        false,
        QStringLiteral("missing"),
        QStringLiteral("missing"),
        false,
        kiriview::KioOperationFailureCause::Backend,
    };
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData()),
        decodeJobCallbacks({},
            [&errorRequests, &deliveredError](const kiriview::ImageDecodeRequest& request,
                const kiriview::ImageDataLoadError& error) {
                errorRequests.push_back(request);
                deliveredError = error;
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(3, imageUrl));
    dataLoader.failFrontLoad(kiriview::ImageDataLoadError { expectedFailure });

    QCOMPARE(errorRequests.size(), std::size_t(1));
    QCOMPARE(errorRequests.front().id(), quint64(3));
    QVERIFY(deliveredError.has_value());
    const auto* failure = std::get_if<kiriview::KioOperationFailure>(&*deliveredError);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->operationKind, expectedFailure.operationKind);
    QCOMPARE(failure->targetUrl, expectedFailure.targetUrl);
    QCOMPARE(failure->rawErrorCode, expectedFailure.rawErrorCode);
    QCOMPARE(failure->canceled, expectedFailure.canceled);
    QCOMPARE(failure->userMessage, expectedFailure.userMessage);
    QCOMPARE(failure->diagnosticDetail, expectedFailure.diagnosticDetail);
    QCOMPARE(failure->retryable, expectedFailure.retryable);
    QCOMPARE(failure->cause, expectedFailure.cause);
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::unmanagedLoadedDataOverBudgetReturnsTypedResourceFailure()
{
    ManualImageDataLoader dataLoader;
    auto budget = std::make_shared<kiriview::ImageSourceDataBudget>(16, 16);
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData());
    dependencies.sourceDataBudget = budget;
    std::optional<kiriview::ImageDataLoadError> deliveredError;
    int decodedCount = 0;
    const QUrl imageUrl = indexedImageUrl(4);
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            [&deliveredError](const kiriview::ImageDecodeRequest&,
                const kiriview::ImageDataLoadError& error) { deliveredError = error; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(4, imageUrl));
    dataLoader.finishFrontLoad(kiriview::ImageSourceData(QByteArray(32, 'x')));

    QCOMPARE(decodedCount, 0);
    QVERIFY(deliveredError.has_value());
    const auto* failure = std::get_if<kiriview::KioOperationFailure>(&*deliveredError);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->operationKind, kiriview::KioOperationKind::ImageDataRead);
    QCOMPARE(failure->targetUrl, imageUrl);
    QCOMPARE(failure->cause, kiriview::KioOperationFailureCause::ResourceLimitExceeded);
    QVERIFY(!failure->rawErrorCode.has_value());
    QVERIFY(!failure->canceled);
    QVERIFY(!failure->diagnosticDetail.isEmpty());
    QVERIFY(!failure->retryable);
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::decodeErrorsAreDeliveredAsResults()
{
    ManualImageDataLoader dataLoader;
    std::optional<kiriview::DecodedImageResult> decodedResult;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData()),
        decodeJobCallbacks(
            [&decodedResult](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult result) {
                decodedResult = std::move(result);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(4, indexedImageUrl(4)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("bad"));

    QTRY_VERIFY(decodedResult.has_value());
    const auto* failure = kiriview::decodedImageResultFailure(*decodedResult);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->errorString, testImageDecodeFailureString());
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::decodeRequestIsPassedToDecoder()
{
    ManualImageDataLoader dataLoader;
    std::optional<kiriview::ImageDecodeRequest> decoderRequest;
    const QUrl imageUrl = indexedImageUrl(5);
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesFor(dataLoader,
            [&decoderRequest](const QByteArray&, const kiriview::ImageDecodeRequest& request) {
                decoderRequest = request;
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage());
            }),
        decodeJobCallbacks([](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult) {}));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        5, imageUrl, kiriview::ImageFirstDisplayDecodeContext { QSize(320, 200) }));
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));

    QTRY_VERIFY(decoderRequest.has_value());
    QCOMPARE(decoderRequest->id(), quint64(5));
    QCOMPARE(decoderRequest->imageUrl(), imageUrl);
    QCOMPARE(decoderRequest->firstDisplay().logicalViewportSize, QSize(320, 200));
}

void TestImageDecodeJob::decodeWorkerSchedulerCanBeDrivenManually()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    bool decoderCalled = false;
    std::vector<kiriview::ImageDecodeRequest> decodedRequests;
    kiriview::ImageDecodeDependencies dependencies = imageDecodeDependenciesFor(
        dataLoader, [&decoderCalled](const QByteArray&, const kiriview::ImageDecodeRequest&) {
            decoderCalled = true;
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        });
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            [&decodedRequests](kiriview::ImageDecodeRequest request, kiriview::DecodedImageResult) {
                decodedRequests.push_back(std::move(request));
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(16, indexedImageUrl(16)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QVERIFY(!decoderCalled);
    QCOMPARE(decodedRequests.size(), std::size_t(0));

    workerScheduler.runWork(0);
    QVERIFY(!decoderCalled);
    QCOMPARE(decodedRequests.size(), std::size_t(0));

    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    QVERIFY(!decoderCalled);

    workerScheduler.runWork(1);
    QVERIFY(decoderCalled);
    QCOMPARE(decodedRequests.size(), std::size_t(0));

    workerScheduler.finish(1);

    QCOMPARE(decodedRequests.size(), std::size_t(1));
    QCOMPARE(decodedRequests.front().id(), quint64(16));
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::temporaryWorkspaceContentionWaitsWithoutFailure()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(10, 10);
    kiriview::ImageDecodeWorkspaceLease blocker
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(blocker, 10));
    int producerCount = 0;
    std::optional<kiriview::DecodedImageResult> decodedResult;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.workspaceBudget = workspaceBudget;
    dependencies.dataPlanner
        = [&producerCount](kiriview::ImageSourceData sourceData,
              const kiriview::ImageDecodeRequest&, kiriview::ImageDecodeWorkspacePriority priority)
        -> kiriview::PreparedImageDecodeResult {
        auto execute = [&producerCount, sourceData = std::move(sourceData)](
                           kiriview::ImageDecodeWorkspaceLease) mutable
            -> kiriview::PreparedImageDecodeResult {
            ++producerCount;
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        };
        return std::make_unique<kiriview::PreparedImageDecodeWork>(
            kiriview::ImageDecodeWorkspaceAdmissionRequest { 5, 0, priority },
            preparedHardLimitTestFailure(), std::move(execute));
    };
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            [&decodedResult](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult result) {
                decodedResult = std::move(result);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(31, indexedImageUrl(31)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCoreApplication::processEvents();

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(producerCount, 0);
    QVERIFY(!decodedResult.has_value());
    QVERIFY(decodeJob.hasActiveRequest());

    blocker = {};
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    QCOMPARE(producerCount, 0);
    workerScheduler.runWork(1);
    workerScheduler.finish(1);

    QCOMPARE(producerCount, 1);
    QVERIFY(decodedResult.has_value());
    QVERIFY(decodedResult->has_value());
}

void TestImageDecodeJob::intrinsicWorkspaceRejectionPreservesTypedFailure()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(10, 6);
    int producerCount = 0;
    std::optional<kiriview::DecodedImageResult> decodedResult;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.workspaceBudget = workspaceBudget;
    dependencies.dataPlanner
        = [&producerCount](kiriview::ImageSourceData sourceData,
              const kiriview::ImageDecodeRequest&, kiriview::ImageDecodeWorkspacePriority priority)
        -> kiriview::PreparedImageDecodeResult {
        auto execute = [&producerCount, sourceData = std::move(sourceData)](
                           kiriview::ImageDecodeWorkspaceLease) mutable
            -> kiriview::PreparedImageDecodeResult {
            ++producerCount;
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        };
        return std::make_unique<kiriview::PreparedImageDecodeWork>(
            kiriview::ImageDecodeWorkspaceAdmissionRequest { 7, 0, priority },
            preparedHardLimitTestFailure(), std::move(execute));
    };
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            [&decodedResult](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult result) {
                decodedResult = std::move(result);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(32, indexedImageUrl(32)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(producerCount, 0);
    QVERIFY(decodedResult.has_value());
    const kiriview::DecodedImageFailure* failure
        = kiriview::decodedImageResultFailure(*decodedResult);
    QVERIFY(failure != nullptr);
    QCOMPARE(failure->route, kiriview::DecodedImageFailureRoute::Svg);
    QCOMPARE(failure->operation, kiriview::DecodedImageFailureOperation::DecodeFirstDisplayImage);
    QCOMPARE(
        failure->diagnosticDetail, QStringLiteral("prepared SVG raster exceeds its hard envelope"));
    QCOMPARE(failure->cause, kiriview::DecodedImageFailureCause::ResourceLimitExceeded);
    QVERIFY(!failure->retryable);
}

void TestImageDecodeJob::cancelingPendingAdmissionSuppressesProducerAndPublication()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(10, 10);
    kiriview::ImageDecodeWorkspaceLease blocker
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(blocker, 10));
    int producerCount = 0;
    int decodedCount = 0;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.workspaceBudget = workspaceBudget;
    dependencies.dataPlanner
        = [&producerCount](kiriview::ImageSourceData sourceData,
              const kiriview::ImageDecodeRequest&, kiriview::ImageDecodeWorkspacePriority priority)
        -> kiriview::PreparedImageDecodeResult {
        auto execute = [&producerCount, sourceData = std::move(sourceData)](
                           kiriview::ImageDecodeWorkspaceLease) mutable
            -> kiriview::PreparedImageDecodeResult {
            ++producerCount;
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        };
        return std::make_unique<kiriview::PreparedImageDecodeWork>(
            kiriview::ImageDecodeWorkspaceAdmissionRequest { 5, 0, priority },
            preparedHardLimitTestFailure(), std::move(execute));
    };
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(33, indexedImageUrl(33)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QCoreApplication::processEvents();
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));

    decodeJob.cancel();
    blocker = {};
    QCoreApplication::processEvents();

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(producerCount, 0);
    QCOMPARE(decodedCount, 0);
    QVERIFY(!decodeJob.hasActiveRequest());
}

void TestImageDecodeJob::retirementWaitsForRunningProducerAndFiresOnce()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    int producerCount = 0;
    int decodedCount = 0;
    int retiredCount = 0;
    QThread* retiredThread = nullptr;
    kiriview::ImageDecodeDependencies dependencies = imageDecodeDependenciesFor(
        dataLoader, [&producerCount](const QByteArray&, const kiriview::ImageDecodeRequest&) {
            ++producerCount;
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        });
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {}, {},
            [&retiredCount, &retiredThread](const kiriview::ImageDecodeRequest&) {
                ++retiredCount;
                retiredThread = QThread::currentThread();
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(34, indexedImageUrl(34)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("ok"));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));

    workerScheduler.runWork(1);
    QCOMPARE(producerCount, 1);
    decodeJob.cancel();
    QCoreApplication::processEvents();
    QCOMPARE(decodedCount, 0);
    QCOMPARE(retiredCount, 0);

    workerScheduler.finish(1);
    QTRY_COMPARE(retiredCount, 1);
    QCOMPARE(retiredThread, thread());
    decodeJob.cancel();
    QCoreApplication::processEvents();
    QCOMPARE(retiredCount, 1);
}

void TestImageDecodeJob::synchronousDependenciesRetireExactlyOnceAfterPublication()
{
    int decodedCount = 0;
    int retiredCount = 0;
    bool publicationPrecededRetirement = false;
    kiriview::ImageDecodeDependencies dependencies;
    dependencies.dataLoader
        = [](QObject*, kiriview::ImageDecodeRequest, kiriview::ImageDataCallback callback,
              kiriview::ImageDataLoadErrorCallback) {
              callback(kiriview::ImageSourceData(QByteArrayLiteral("ok")));
              return kiriview::ImageIoJob {};
          };
    dependencies.dataPlanner
        = [](kiriview::ImageSourceData sourceData, const kiriview::ImageDecodeRequest&,
              kiriview::ImageDecodeWorkspacePriority priority)
        -> kiriview::PreparedImageDecodeResult {
        auto execute
            = [sourceData = std::move(sourceData)](kiriview::ImageDecodeWorkspaceLease) mutable
            -> kiriview::PreparedImageDecodeResult {
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        };
        return std::make_unique<kiriview::PreparedImageDecodeWork>(
            kiriview::ImageDecodeWorkspaceAdmissionRequest { 1, 0, priority },
            preparedHardLimitTestFailure(), std::move(execute));
    };
    dependencies.workerScheduler
        = kiriview::ImageWorkerScheduler([](QObject*, kiriview::ImageWorkerOperation operation,
                                             kiriview::ImageWorkerCompletion completion) {
              operation();
              completion();
              return kiriview::ImageWorkerTask {};
          });
    dependencies.workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(8, 8);
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {}, {},
            [&decodedCount, &retiredCount, &publicationPrecededRetirement](
                const kiriview::ImageDecodeRequest&) {
                publicationPrecededRetirement = decodedCount == 1;
                ++retiredCount;
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(35, indexedImageUrl(35)));

    QTRY_COMPARE(decodedCount, 1);
    QTRY_COMPARE(retiredCount, 1);
    QVERIFY(publicationPrecededRetirement);
    QCoreApplication::processEvents();
    QCOMPARE(retiredCount, 1);
}

void TestImageDecodeJob::synchronousPlannerCompletionAllowsCallbackToDestroyJob()
{
    int scheduleCount = 0;
    int decodedCount = 0;
    int retiredCount = 0;
    kiriview::ImageDecodeDependencies dependencies;
    dependencies.dataLoader
        = [](QObject*, kiriview::ImageDecodeRequest, kiriview::ImageDataCallback callback,
              kiriview::ImageDataLoadErrorCallback) {
              callback(kiriview::ImageSourceData(QByteArrayLiteral("ok")));
              return kiriview::ImageIoJob {};
          };
    dependencies.dataPlanner
        = [](kiriview::ImageSourceData, const kiriview::ImageDecodeRequest&,
              kiriview::ImageDecodeWorkspacePriority) -> kiriview::PreparedImageDecodeResult {
        return kiriview::successfulDecodedImageResult(
            kiriview::TestSupport::staticDecodedTestImage());
    };
    dependencies.workerScheduler = kiriview::ImageWorkerScheduler(
        [&scheduleCount](QObject*, kiriview::ImageWorkerOperation operation,
            kiriview::ImageWorkerCompletion completion) {
            ++scheduleCount;
            operation();
            completion();
            return kiriview::ImageWorkerTask {};
        });

    QPointer<kiriview::ImageDecodeJob> guardedJob;
    guardedJob = new kiriview::ImageDecodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            [&guardedJob, &decodedCount](
                kiriview::ImageDecodeRequest, kiriview::DecodedImageResult) {
                ++decodedCount;
                delete guardedJob.data();
            },
            {}, {}, [&retiredCount](const kiriview::ImageDecodeRequest&) { ++retiredCount; }));

    guardedJob->start(kiriview::ImageDecodeRequest::fromUrl(36, indexedImageUrl(36)));

    QVERIFY(guardedJob.isNull());
    QCOMPARE(scheduleCount, 1);
    QCOMPARE(decodedCount, 1);
    QTRY_COMPARE(retiredCount, 1);
}

void TestImageDecodeJob::synchronousPreviewCompletionAllowsCallbackToDestroyJob()
{
    ManualImageDataLoader dataLoader;
    int scheduleCount = 0;
    int decodedCount = 0;
    int previewCount = 0;
    int retiredCount = 0;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.thumbnailPreviewLookupProvider
        = [](QObject*, kiriview::ThumbnailCacheLookupRequest,
              kiriview::ThumbnailCacheLookupCallback callback) {
              callback(readyThumbnailLookup(thumbnailImage(QSize(16, 16))));
              return kiriview::ImageIoJob {};
          };
    dependencies.workerScheduler
        = kiriview::ImageWorkerScheduler([&scheduleCount](QObject*, kiriview::ImageWorkerOperation,
                                             kiriview::ImageWorkerCompletion) {
              ++scheduleCount;
              return kiriview::ImageWorkerTask {};
          });

    QPointer<kiriview::ImageDecodeJob> guardedJob;
    guardedJob = new kiriview::ImageDecodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&guardedJob, &previewCount](
                const kiriview::ImageDecodeRequest&, kiriview::StaticDisplayImagePayload) {
                ++previewCount;
                delete guardedJob.data();
            },
            [&retiredCount](const kiriview::ImageDecodeRequest&) { ++retiredCount; }));

    guardedJob->start(kiriview::ImageDecodeRequest::fromUrl(
        37, QUrl::fromLocalFile(QStringLiteral("/tmp/synchronous-preview.png"))));
    dataLoader.finishFrontLoad(encodedPngData(QSize(32, 32)));

    QVERIFY(guardedJob.isNull());
    QCOMPARE(scheduleCount, 0);
    QCOMPARE(decodedCount, 0);
    QCOMPARE(previewCount, 1);
    QTRY_COMPARE(retiredCount, 1);
}

void TestImageDecodeJob::retainedDecodedPayloadRetainsSourceDataReservation()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    auto budget = std::make_shared<kiriview::ImageSourceDataBudget>(16, 16);
    std::optional<kiriview::DecodedImageResult> retainedResult;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            [&retainedResult](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult result) {
                retainedResult = std::move(result);
            }));

    kiriview::ImageSourceDataLease lease = budget->startLease();
    QVERIFY(lease.tryReserve(2));
    kiriview::ImageSourceData sourceData(QByteArrayLiteral("ok"), std::move(lease));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(17, indexedImageUrl(17)));
    dataLoader.finishFrontLoad(std::move(sourceData));
    QCOMPARE(budget->reservedByteCount(), qsizetype(2));

    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);

    QVERIFY(retainedResult.has_value());
    QVERIFY(retainedResult->has_value());
    QCOMPARE(budget->reservedByteCount(), qsizetype(2));

    retainedResult.reset();
    QCOMPARE(budget->reservedByteCount(), qsizetype(0));
}

void TestImageDecodeJob::sourceReplacementCancelsQueuedDecodeWork()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    std::vector<kiriview::ImageDecodeRequest> decoderRequests;
    std::vector<kiriview::ImageDecodeRequest> decodedRequests;
    kiriview::ImageDecodeDependencies dependencies = imageDecodeDependenciesFor(dataLoader,
        [&decoderRequests](const QByteArray&, const kiriview::ImageDecodeRequest& request) {
            decoderRequests.push_back(request);
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage());
        });
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            [&decodedRequests](kiriview::ImageDecodeRequest request, kiriview::DecodedImageResult) {
                decodedRequests.push_back(std::move(request));
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(21, indexedImageUrl(21)));
    dataLoader.finishFrontLoad(QByteArrayLiteral("first"));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(22, indexedImageUrl(22)));
    dataLoader.finishBackLoad(QByteArrayLiteral("second"));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));

    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QVERIFY(decoderRequests.empty());
    QVERIFY(decodedRequests.empty());

    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    workerScheduler.runWork(2);
    workerScheduler.finish(2);
    QCOMPARE(decoderRequests.size(), std::size_t(1));
    QCOMPARE(decoderRequests.front().id(), quint64(22));
    QCOMPARE(decodedRequests.size(), std::size_t(1));
    QCOMPARE(decodedRequests.front().id(), quint64(22));
}

void TestImageDecodeJob::xdgThumbnailPreviewIsDeliveredBeforeDecodeCompletes()
{
    const QByteArray data = encodedPngData(QSize(800, 600));
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    QSemaphore decoderMayFinish;
    std::optional<kiriview::StaticDisplayImagePayload> previewPayload;
    int decodedCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesForThumbnailPreview(
            dataLoader,
            [&decoderMayFinish](const QByteArray&, const kiriview::ImageDecodeRequest&) {
                decoderMayFinish.acquire();
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage(
                        kiriview::TestSupport::testImage(800, 600)));
            },
            thumbnailLookup),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewPayload](
                const kiriview::ImageDecodeRequest&, kiriview::StaticDisplayImagePayload payload) {
                previewPayload = std::move(payload);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(8, indexedImageUrl(8)));
    dataLoader.finishFrontLoad(data);

    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    QCOMPARE(thumbnailLookup.requests.front().requestedBucket,
        kiriview::ActiveNavigationThumbnailDemandBucket::XXLarge);
    QCOMPARE(thumbnailLookup.requests.front().workspacePriority,
        kiriview::ImageDecodeWorkspacePriority::Demanded);
    thumbnailLookup.finish(0, readyThumbnailLookup());

    QTRY_VERIFY(previewPayload.has_value());
    QCOMPARE(previewPayload->quality, kiriview::DisplayImageQuality::ThumbnailPreview);
    QCOMPARE(previewPayload->previewOrigin, kiriview::DisplayImagePreviewOrigin::XdgThumbnail);
    QCOMPARE(previewPayload->originalSize, QSize(800, 600));
    QCOMPARE(previewPayload->image.size(), QSize(400, 300));
    QCOMPARE(decodedCount, 0);

    decoderMayFinish.release();
    QTRY_COMPARE(decodedCount, 1);
}

void TestImageDecodeJob::staleXdgThumbnailPreviewIsIgnored()
{
    const QByteArray data = encodedPngData(QSize(800, 600));
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    QSemaphore decoderMayFinish;
    std::atomic<int> decoderCalls = 0;
    int previewCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesForThumbnailPreview(
            dataLoader,
            [&decoderMayFinish, &decoderCalls](
                const QByteArray&, const kiriview::ImageDecodeRequest&) {
                ++decoderCalls;
                decoderMayFinish.acquire();
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage(
                        kiriview::TestSupport::testImage(800, 600)));
            },
            thumbnailLookup),
        decodeJobCallbacks([](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult) {}, {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(9, indexedImageUrl(9)));
    dataLoader.finishFrontLoad(data);
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(10, indexedImageUrl(10)));
    thumbnailLookup.finish(0, readyThumbnailLookup());

    QCOMPARE(previewCount, 0);
    decoderMayFinish.release(2);
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
}

void TestImageDecodeJob::nonRawXdgMissDoesNotRunRawEmbeddedPreview()
{
    const QByteArray data = encodedPngData(QSize(800, 600));
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    QSemaphore decoderMayFinish;
    int decodedCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesForThumbnailPreview(
            dataLoader,
            [&decoderMayFinish](const QByteArray&, const kiriview::ImageDecodeRequest&) {
                decoderMayFinish.acquire();
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage(
                        kiriview::TestSupport::testImage(800, 600)));
            },
            thumbnailLookup, &rawExtractor),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {}, [](const kiriview::ImageDecodeRequest&, kiriview::StaticDisplayImagePayload) {}));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(13, indexedImageUrl(13)));
    dataLoader.finishFrontLoad(data);

    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    thumbnailLookup.finish(0, missingThumbnailLookup());

    QCOMPARE(rawExtractor.callCount(), 0);
    QCOMPARE(decodedCount, 0);

    decoderMayFinish.release();
    QTRY_COMPARE(decodedCount, 1);
}

void TestImageDecodeJob::rawEmbeddedPreviewIsDeliveredAfterXdgMiss()
{
    MinimumThreadPoolConcurrency threadPoolConcurrency(2);
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    QSemaphore decoderMayFinish;
    SemaphoreReleaseOnExit releaseDecoderOnExit(decoderMayFinish);
    std::optional<kiriview::StaticDisplayImagePayload> previewPayload;
    int decodedCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesForThumbnailPreview(
            dataLoader,
            [&decoderMayFinish](const QByteArray&, const kiriview::ImageDecodeRequest&) {
                decoderMayFinish.acquire();
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage(
                        kiriview::TestSupport::testImage(32, 32)));
            },
            thumbnailLookup, &rawExtractor),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewPayload](
                const kiriview::ImageDecodeRequest&, kiriview::StaticDisplayImagePayload payload) {
                previewPayload = std::move(payload);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        11, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);

    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    QCOMPARE(rawExtractor.callCount(), 0);

    thumbnailLookup.finish(0, missingThumbnailLookup());

    QTRY_COMPARE(rawExtractor.callCount(), 1);
    const std::vector<kiriview::ImageDecodeRequest> rawRequests = rawExtractor.requests();
    const std::vector<qsizetype> rawDataSizes = rawExtractor.dataSizes();
    const std::vector<qsizetype> rawWorkspaceByteCounts = rawExtractor.workspaceByteCounts();
    QCOMPARE(rawRequests.front().id(), quint64(11));
    QCOMPARE(rawDataSizes.front(), data.size());
    QCOMPARE(
        rawWorkspaceByteCounts.front(), kiriview::rawEmbeddedThumbnailPreviewWorkspaceByteCount());
    QTRY_VERIFY(previewPayload.has_value());
    QCOMPARE(previewPayload->quality, kiriview::DisplayImageQuality::ThumbnailPreview);
    QCOMPARE(
        previewPayload->previewOrigin, kiriview::DisplayImagePreviewOrigin::RawEmbeddedThumbnail);
    QCOMPARE(previewPayload->originalSize, QSize(32, 32));
    QCOMPARE(previewPayload->image.size(), QSize(16, 16));
    QVERIFY(previewPayload->refinementSource == nullptr);
    QCOMPARE(decodedCount, 0);

    decoderMayFinish.release();
    releaseDecoderOnExit.dismiss();
    QTRY_COMPARE(decodedCount, 1);
}

void TestImageDecodeJob::rawEmbeddedPreviewContentionWaitsBeforeWorker()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    ManualImageWorkerScheduler workerScheduler;
    const qsizetype workspaceByteCount = kiriview::rawEmbeddedThumbnailPreviewWorkspaceByteCount();
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        workspaceByteCount, workspaceByteCount);
    int previewCount = 0;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesForThumbnailPreview(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(), thumbnailLookup, &rawExtractor);
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.workspaceBudget = workspaceBudget;
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks({}, {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        41, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));

    kiriview::ImageDecodeWorkspaceLease blocker
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(blocker, workspaceByteCount));
    thumbnailLookup.finish(0, missingThumbnailLookup());
    QCoreApplication::processEvents();

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    QCOMPARE(rawExtractor.callCount(), 0);
    QCOMPARE(previewCount, 0);

    blocker = {};
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    workerScheduler.runWork(2);
    workerScheduler.finish(2);

    QCOMPARE(rawExtractor.callCount(), 1);
    QCOMPARE(previewCount, 1);
    QCOMPARE(rawExtractor.workspaceByteCounts().front(), workspaceByteCount);
    decodeJob.cancel();
}

void TestImageDecodeJob::cancelingPendingRawEmbeddedPreviewSuppressesWorker()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    ManualImageWorkerScheduler workerScheduler;
    const qsizetype workspaceByteCount = kiriview::rawEmbeddedThumbnailPreviewWorkspaceByteCount();
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        workspaceByteCount, workspaceByteCount);
    int previewCount = 0;
    int retiredCount = 0;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesForThumbnailPreview(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(), thumbnailLookup, &rawExtractor);
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.workspaceBudget = workspaceBudget;
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks(
            {}, {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; },
            [&retiredCount](const kiriview::ImageDecodeRequest&) { ++retiredCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        42, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));

    kiriview::ImageDecodeWorkspaceLease blocker
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(blocker, workspaceByteCount));
    thumbnailLookup.finish(0, missingThumbnailLookup());
    QCoreApplication::processEvents();
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));

    decodeJob.cancel();
    blocker = {};
    QCoreApplication::processEvents();

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    QCOMPARE(rawExtractor.callCount(), 0);
    QCOMPARE(previewCount, 0);
    QTRY_COMPARE(retiredCount, 1);
}

void TestImageDecodeJob::terminalDecodeCancelsPendingPreviewAdmission()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualImageWorkerScheduler workerScheduler;
    const qsizetype workspaceByteCount = kiriview::rawImageOpenWorkspaceByteCount;
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        workspaceByteCount, workspaceByteCount);
    kiriview::ImageDecodeWorkspaceLease blocker
        = kiriview::ImageDecodeWorkspaceDetail::startLease(*workspaceBudget);
    QVERIFY(kiriview::ImageDecodeWorkspaceDetail::tryReserve(blocker, workspaceByteCount));
    auto sourceDataBudget
        = std::make_shared<kiriview::ImageSourceDataBudget>(data.size(), data.size());
    kiriview::ImageSourceDataLease sourceDataLease = sourceDataBudget->startLease();
    QVERIFY(sourceDataLease.tryReserve(data.size()));

    int decodedCount = 0;
    int previewCount = 0;
    int retiredCount = 0;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, kiriview::TestSupport::staticImageDataDecoder());
    dependencies.dataPlanner
        = [](kiriview::ImageSourceData, const kiriview::ImageDecodeRequest&,
              kiriview::ImageDecodeWorkspacePriority) -> kiriview::PreparedImageDecodeResult {
        return kiriview::successfulDecodedImageResult(
            kiriview::TestSupport::staticDecodedTestImage());
    };
    dependencies.thumbnailPreviewLookupProvider = thumbnailLookup.provider();
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.sourceDataBudget = sourceDataBudget;
    dependencies.workspaceBudget = workspaceBudget;
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; },
            [&retiredCount](const kiriview::ImageDecodeRequest&) { ++retiredCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        45, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(kiriview::ImageSourceData(data, std::move(sourceDataLease)));

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(thumbnailLookup.requests.size(), std::size_t(0));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);

    QCOMPARE(decodedCount, 1);
    QCOMPARE(previewCount, 0);
    QTRY_COMPARE(retiredCount, 1);
    QCOMPARE(sourceDataBudget->reservedByteCount(), qsizetype(0));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    blocker = {};
}

void TestImageDecodeJob::intrinsicPreviewLimitSkipsWithoutFailingDecode()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    ManualImageWorkerScheduler workerScheduler;
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(
        kiriview::rawImageOpenWorkspaceByteCount - 1, kiriview::rawImageOpenWorkspaceByteCount - 1);
    int decodedCount = 0;
    int loadErrorCount = 0;
    int previewCount = 0;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesForThumbnailPreview(dataLoader,
            kiriview::TestSupport::staticImageDataDecoder(), thumbnailLookup, &rawExtractor);
    dependencies.workerScheduler = workerScheduler.scheduler();
    dependencies.workspaceBudget = std::move(workspaceBudget);
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            [&loadErrorCount](const kiriview::ImageDecodeRequest&,
                const kiriview::ImageDataLoadError&) { ++loadErrorCount; },
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        43, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);

    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(thumbnailLookup.requests.size(), std::size_t(0));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);

    QCOMPARE(decodedCount, 1);
    QCOMPARE(loadErrorCount, 0);
    QCOMPARE(previewCount, 0);
    QCOMPARE(rawExtractor.callCount(), 0);
}

void TestImageDecodeJob::canceledRunningRawPreviewRetainsSourceUntilPhysicalRetirement()
{
    MinimumThreadPoolConcurrency threadPoolConcurrency(4);
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    QSemaphore decoderStarted;
    QSemaphore decoderMayReturn;
    QSemaphore rawStarted;
    QSemaphore rawMayReturn;
    SemaphoreReleaseOnExit releaseDecoderOnExit(decoderMayReturn);
    SemaphoreReleaseOnExit releaseRawOnExit(rawMayReturn);
    rawExtractor.started = &rawStarted;
    rawExtractor.mayReturn = &rawMayReturn;
    auto sourceDataBudget
        = std::make_shared<kiriview::ImageSourceDataBudget>(data.size(), data.size());
    int decodedCount = 0;
    int previewCount = 0;
    int retiredCount = 0;
    kiriview::ImageDecodeDependencies dependencies = imageDecodeDependenciesForThumbnailPreview(
        dataLoader,
        [&decoderStarted, &decoderMayReturn](
            const QByteArray&, const kiriview::ImageDecodeRequest&) {
            decoderStarted.release();
            decoderMayReturn.acquire();
            return kiriview::failedDecodedImageResult(QStringLiteral("decode finished"));
        },
        thumbnailLookup, &rawExtractor);
    dependencies.sourceDataBudget = sourceDataBudget;
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; },
            [&retiredCount](const kiriview::ImageDecodeRequest&) { ++retiredCount; }));

    kiriview::ImageSourceDataLease sourceDataLease = sourceDataBudget->startLease();
    QVERIFY(sourceDataLease.tryReserve(data.size()));
    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        44, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(kiriview::ImageSourceData(data, std::move(sourceDataLease)));
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    thumbnailLookup.finish(0, missingThumbnailLookup());

    QTRY_VERIFY_WITH_TIMEOUT(rawStarted.available() > 0, 5000);
    rawStarted.acquire();
    QTRY_VERIFY_WITH_TIMEOUT(decoderStarted.available() > 0, 5000);
    decoderStarted.acquire();
    decoderMayReturn.release();
    releaseDecoderOnExit.dismiss();
    QTRY_COMPARE(decodedCount, 1);
    decodeJob.cancel();

    QCOMPARE(previewCount, 0);
    QCOMPARE(sourceDataBudget->reservedByteCount(), data.size());
    rawMayReturn.release();
    releaseRawOnExit.dismiss();
    QTRY_COMPARE(retiredCount, 1);
    QTRY_COMPARE(sourceDataBudget->reservedByteCount(), qsizetype(0));
    QCOMPARE(previewCount, 0);
}

void TestImageDecodeJob::rawEmbeddedPreviewMissDoesNotPublish()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    rawExtractor.result.status = kiriview::RawEmbeddedThumbnailPreviewStatus::Missing;
    rawExtractor.result.image = {};
    ManualImageWorkerScheduler workerScheduler;
    int previewCount = 0;
    int decodedCount = 0;
    kiriview::ImageDecodeDependencies dependencies = imageDecodeDependenciesForThumbnailPreview(
        dataLoader,
        [](const QByteArray&, const kiriview::ImageDecodeRequest&) {
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage(
                    kiriview::TestSupport::testImage(32, 32)));
        },
        thumbnailLookup, &rawExtractor);
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        14, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);

    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    thumbnailLookup.finish(0, missingThumbnailLookup());

    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    workerScheduler.runWork(2);
    workerScheduler.finish(2);
    QCOMPARE(rawExtractor.callCount(), 1);
    QCOMPARE(previewCount, 0);
    QCOMPARE(decodedCount, 0);

    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(4));
    workerScheduler.runWork(3);
    workerScheduler.finish(3);
    QCOMPARE(decodedCount, 1);
}

void TestImageDecodeJob::lateRawEmbeddedPreviewAfterDecodeIsIgnored()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    ManualImageWorkerScheduler workerScheduler;
    int previewCount = 0;
    int decodedCount = 0;
    kiriview::ImageDecodeDependencies dependencies = imageDecodeDependenciesForThumbnailPreview(
        dataLoader,
        [](const QByteArray&, const kiriview::ImageDecodeRequest&) {
            return kiriview::successfulDecodedImageResult(
                kiriview::TestSupport::staticDecodedTestImage(
                    kiriview::TestSupport::testImage(32, 32)));
        },
        thumbnailLookup, &rawExtractor);
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImageDecodeJob decodeJob(this, std::move(dependencies),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewCount](const kiriview::ImageDecodeRequest&,
                kiriview::StaticDisplayImagePayload) { ++previewCount; }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        15, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);

    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    thumbnailLookup.finish(0, missingThumbnailLookup());

    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(4));
    workerScheduler.runWork(3);
    workerScheduler.finish(3);
    QCOMPARE(decodedCount, 1);

    workerScheduler.runWork(2);
    workerScheduler.finish(2);
    QCOMPARE(rawExtractor.callCount(), 1);
    QCOMPARE(previewCount, 0);
}

void TestImageDecodeJob::readyXdgThumbnailPreviewSuppressesRawEmbeddedPreview()
{
    const QByteArray data = rawFixtureData();
    QVERIFY(!data.isEmpty());

    ManualImageDataLoader dataLoader;
    ManualThumbnailLookupProvider thumbnailLookup;
    ManualRawEmbeddedThumbnailPreviewExtractor rawExtractor;
    QSemaphore decoderMayFinish;
    std::optional<kiriview::StaticDisplayImagePayload> previewPayload;
    int decodedCount = 0;
    kiriview::ImageDecodeJob decodeJob(this,
        imageDecodeDependenciesForThumbnailPreview(
            dataLoader,
            [&decoderMayFinish](const QByteArray&, const kiriview::ImageDecodeRequest&) {
                decoderMayFinish.acquire();
                return kiriview::successfulDecodedImageResult(
                    kiriview::TestSupport::staticDecodedTestImage(
                        kiriview::TestSupport::testImage(32, 32)));
            },
            thumbnailLookup, &rawExtractor),
        decodeJobCallbacks([&decodedCount](kiriview::ImageDecodeRequest,
                               kiriview::DecodedImageResult) { ++decodedCount; },
            {},
            [&previewPayload](
                const kiriview::ImageDecodeRequest&, kiriview::StaticDisplayImagePayload payload) {
                previewPayload = std::move(payload);
            }));

    decodeJob.start(kiriview::ImageDecodeRequest::fromUrl(
        12, QUrl::fromLocalFile(QStringLiteral("/tmp/raw-cfa-smoke.dng"))));
    dataLoader.finishFrontLoad(data);

    QTRY_COMPARE(thumbnailLookup.requests.size(), std::size_t(1));
    thumbnailLookup.finish(0, readyThumbnailLookup(thumbnailImage(QSize(16, 16))));

    QTRY_VERIFY(previewPayload.has_value());
    QCOMPARE(previewPayload->previewOrigin, kiriview::DisplayImagePreviewOrigin::XdgThumbnail);
    QCOMPARE(rawExtractor.callCount(), 0);
    QCOMPARE(decodedCount, 0);

    decoderMayFinish.release();
    QTRY_COMPARE(decodedCount, 1);
}

QTEST_GUILESS_MAIN(TestImageDecodeJob)

#include "tst_imagedecodejob.moc"
