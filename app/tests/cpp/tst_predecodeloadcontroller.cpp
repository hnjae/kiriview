// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/imagedecodejob.h"
#include "image_test_support.h"
#include "predecode/predecodeloadcontroller.h"

#include <QCoreApplication>
#include <QObject>
#include <QSize>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using kiriview::TestSupport::imageDecodeDependenciesFor;
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::ManualImageDataLoader;
using kiriview::TestSupport::ManualImageWorkerScheduler;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::staticImageDataDecoder;
using kiriview::TestSupport::staticImageDataDecoderRejectingBadData;
using kiriview::TestSupport::testImage;
using kiriview::TestSupport::testImageDecodeFailure;

constexpr qsizetype testCacheByteBudget = 1024 * 1024;

struct RetirementControlledWorkerSchedule
{
    kiriview::ImageWorkerOperation work;
    kiriview::ImageWorkerCompletion completion;
    kiriview::ImageWorkerTaskCompletion taskCompletion;
    bool canceled = false;
};

class RetirementControlledWorkerScheduler final
{
public:
    kiriview::ImageWorkerScheduler scheduler()
    {
        return kiriview::ImageWorkerScheduler([this](QObject*, kiriview::ImageWorkerOperation work,
                                                  kiriview::ImageWorkerCompletion completion) {
            auto schedule = std::make_shared<RetirementControlledWorkerSchedule>();
            schedule->work = std::move(work);
            schedule->completion = std::move(completion);
            kiriview::ImageWorkerTask task([weakSchedule = std::weak_ptr(schedule)]() {
                if (const auto active = weakSchedule.lock()) {
                    active->canceled = true;
                }
            });
            schedule->taskCompletion = task.completion();
            m_schedules.push_back(std::move(schedule));
            return task;
        });
    }

    [[nodiscard]] std::size_t scheduleCount() const { return m_schedules.size(); }
    [[nodiscard]] bool canceled(std::size_t index) const { return m_schedules.at(index)->canceled; }
    void runWork(std::size_t index)
    {
        if (m_schedules.at(index)->work) {
            m_schedules.at(index)->work();
        }
    }
    void finish(std::size_t index)
    {
        complete(index);
        m_schedules.at(index)->taskCompletion.retire();
    }
    void complete(std::size_t index)
    {
        const auto& schedule = m_schedules.at(index);
        schedule->taskCompletion.claimAndRun([&]() {
            if (schedule->completion) {
                schedule->completion();
            }
        });
    }
    void retire(std::size_t index)
    {
        const auto& schedule = m_schedules.at(index);
        schedule->work = {};
        schedule->completion = {};
        schedule->taskCompletion.retire();
    }

private:
    std::vector<std::shared_ptr<RetirementControlledWorkerSchedule>> m_schedules;
};

kiriview::DisplayedPredecodeImage displayedImage(const QUrl& url, bool firstDisplay = false)
{
    const kiriview::DisplayImageQuality quality = firstDisplay
        ? kiriview::DisplayImageQuality::FirstDisplay
        : kiriview::DisplayImageQuality::Exact;
    return kiriview::DisplayedPredecodeImage {
        kiriview::DisplayedImageLocation::fromUrl(url),
        true,
        staticDisplayTestImagePayload(testImage(), testImage(), quality),
    };
}

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url)
{
    return kiriview::DisplayedImageLocation::fromUrl(url);
}

std::vector<kiriview::DisplayedImageLocation> displayedLocations(const std::vector<QUrl>& urls)
{
    std::vector<kiriview::DisplayedImageLocation> locations;
    locations.reserve(urls.size());
    for (const QUrl& url : urls) {
        locations.push_back(displayedLocation(url));
    }
    return locations;
}

kiriview::PredecodeLoadWindow loadWindow(
    const QUrl& displayedUrl, std::vector<QUrl> urls, quint64 generation = 7)
{
    return kiriview::PredecodeLoadWindow {
        displayedLocation(displayedUrl),
        displayedLocations(urls),
        { displayedImage(displayedUrl, true) },
        kiriview::ImageFirstDisplayDecodeContext { QSize(640, 480) },
        generation,
        1,
    };
}

kiriview::ImageDataDecodePlanner syntheticPeakPlanner(qsizetype peakByteCount)
{
    return [peakByteCount](kiriview::ImageSourceData sourceData,
               const kiriview::ImageDecodeRequest&, kiriview::ImageDecodeWorkspacePriority priority)
               -> kiriview::PreparedImageDecodeResult {
        auto execute
            = [sourceData = std::move(sourceData)](kiriview::ImageDecodeWorkspaceLease) mutable
            -> kiriview::PreparedImageDecodeResult {
            (void)sourceData;
            return kiriview::TestSupport::failedTestImageDecodeResult();
        };
        return std::make_unique<kiriview::PreparedImageDecodeWork>(
            kiriview::ImageDecodeWorkspaceAdmissionRequest { peakByteCount, 0, priority },
            testImageDecodeFailure(kiriview::DecodedImageFailureRoute::Unknown,
                kiriview::DecodedImageFailureOperation::Unknown,
                QStringLiteral("synthetic workspace limit"), false),
            std::move(execute));
    };
}
}

class TestPredecodeLoadController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windowLoadsCacheDisplayedImageAndPumpQueuedDecodes();
    void parallelLimitStartsMultipleWindowLoads();
    void startWindowLoadsReprioritizesWithoutCancelingActiveDecode();
    void retireBackgroundLoadDropsOnlyMatchingQueuedScope();
    void supersedeBackgroundWindowPreservesActiveWorkAndStopsQueue();
    void canceledDecodeRetainsSlotAndLatestWindowUntilPhysicalRetirement();
    void completedDecodeFailureIsNotRetriedAfterPhysicalRetirement();
    void foregroundOwnerPrecedesWaitingSpeculativeOwnerAfterRetirement();
    void cancelBackgroundWorkSuppressesStaleDecode();
};

void TestPredecodeLoadController::windowLoadsCacheDisplayedImageAndPumpQueuedDecodes()
{
    ManualImageDataLoader dataLoader;
    kiriview::PredecodeLoadController controller(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget);
    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    const QUrl previousUrl = indexedImageUrl(0);

    controller.startWindowLoads(loadWindow(displayedUrl, { displayedUrl, nextUrl, previousUrl }));

    const std::optional<kiriview::PredecodedImage> displayed
        = controller.findPredecodedImage(displayedLocation(displayedUrl));
    QVERIFY(displayed.has_value());
    QCOMPARE(displayed->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QCOMPARE(dataLoader.frontLoad().firstDisplay.logicalViewportSize, QSize(640, 480));

    dataLoader.finishFrontLoad(QByteArrayLiteral("next"));
    QTRY_VERIFY(controller.findPredecodedImage(displayedLocation(nextUrl)).has_value());
    const std::optional<kiriview::PredecodedImage> next
        = controller.findPredecodedImage(displayedLocation(nextUrl));
    QCOMPARE(next->displayImage.sourceIdentity, kiriview::sourceKeyForUrl(nextUrl).identity);
    QCOMPARE(next->displayImage.originalSize, testImage().size());
    QCOMPARE(next->displayImage.image.size(), testImage().size());
    QCOMPARE(next->displayImage.quality, kiriview::DisplayImageQuality::Exact);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, previousUrl);
}

void TestPredecodeLoadController::parallelLimitStartsMultipleWindowLoads()
{
    ManualImageDataLoader dataLoader;
    kiriview::PredecodeLoadController controller(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget);
    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    const QUrl previousUrl = indexedImageUrl(0);
    kiriview::PredecodeLoadWindow window
        = loadWindow(displayedUrl, { displayedUrl, nextUrl, previousUrl });
    window.parallelLimit = 2;

    controller.startWindowLoads(std::move(window));

    QCOMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QCOMPARE(dataLoader.backLoad().url, previousUrl);
}

void TestPredecodeLoadController::startWindowLoadsReprioritizesWithoutCancelingActiveDecode()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies decodeDependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    decodeDependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::PredecodeLoadController controller(
        this, std::move(decodeDependencies), testCacheByteBudget);
    const QUrl staleDisplayedUrl = indexedImageUrl(1);
    const QUrl staleNextUrl = indexedImageUrl(2);
    const QUrl displayedUrl = indexedImageUrl(3);
    const QUrl nextUrl = indexedImageUrl(4);

    controller.startWindowLoads(loadWindow(staleDisplayedUrl, { staleDisplayedUrl, staleNextUrl }));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, staleNextUrl);

    controller.startWindowLoads(loadWindow(displayedUrl, { displayedUrl, nextUrl }, 8));

    QVERIFY(!dataLoader.frontLoad().canceled);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    dataLoader.finishFrontLoad(QByteArrayLiteral("warm"));
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
    QVERIFY(controller.findPredecodedImage(displayedLocation(staleNextUrl)).has_value());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, nextUrl);

    dataLoader.finishBackLoad(QByteArrayLiteral("next"));
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(3));
    workerScheduler.runWork(2);
    workerScheduler.finish(2);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(4));
    workerScheduler.runWork(3);
    workerScheduler.finish(3);
    QVERIFY(controller.findPredecodedImage(displayedLocation(nextUrl)).has_value());
}

void TestPredecodeLoadController::retireBackgroundLoadDropsOnlyMatchingQueuedScope()
{
    ManualImageDataLoader dataLoader;
    kiriview::PredecodeLoadController controller(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget);
    const QUrl requestedUrl = QUrl::fromLocalFile(QStringLiteral("/portal/document/shared.png"));
    const kiriview::DisplayedImageLocation firstLocation
        = kiriview::DisplayedImageLocation::fromResolvedSource(kiriview::ResolvedNavigationSource(
            requestedUrl, {}, QUrl::fromLocalFile(QStringLiteral("/resolved/first/shared.png"))));
    const kiriview::DisplayedImageLocation selectedLocation
        = kiriview::DisplayedImageLocation::fromResolvedSource(kiriview::ResolvedNavigationSource(
            requestedUrl, {}, QUrl::fromLocalFile(QStringLiteral("/resolved/second/shared.png"))));
    QVERIFY(firstLocation != selectedLocation);

    controller.startWindowLoads(kiriview::PredecodeLoadWindow {
        {},
        { firstLocation, selectedLocation },
        {},
        kiriview::ImageFirstDisplayDecodeContext { QSize(640, 480) },
        7,
        1,
    });
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    controller.retireBackgroundLoad(selectedLocation);

    QVERIFY(!dataLoader.frontLoad().canceled);
    dataLoader.finishFrontLoad(QByteArrayLiteral("first scope"));
    QTRY_VERIFY(controller.findPredecodedImage(firstLocation).has_value());
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QVERIFY(!controller.findPredecodedImage(selectedLocation).has_value());
}

void TestPredecodeLoadController::supersedeBackgroundWindowPreservesActiveWorkAndStopsQueue()
{
    ManualImageDataLoader dataLoader;
    kiriview::PredecodeLoadController controller(this,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget);
    const QUrl displayedUrl = indexedImageUrl(0);
    const QUrl activeUrl = indexedImageUrl(1);
    const QUrl queuedUrl = indexedImageUrl(2);

    controller.startWindowLoads(loadWindow(displayedUrl, { activeUrl, queuedUrl }));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, activeUrl);

    controller.supersedeBackgroundWindow();

    QVERIFY(!dataLoader.frontLoad().canceled);
    dataLoader.finishFrontLoad(QByteArrayLiteral("compatible active"));
    QTRY_VERIFY(controller.findPredecodedImage(displayedLocation(activeUrl)).has_value());
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QVERIFY(!controller.findPredecodedImage(displayedLocation(queuedUrl)).has_value());
}

void TestPredecodeLoadController::canceledDecodeRetainsSlotAndLatestWindowUntilPhysicalRetirement()
{
    ManualImageDataLoader dataLoader;
    RetirementControlledWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies decodeDependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    decodeDependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::PredecodeLoadController controller(
        this, std::move(decodeDependencies), testCacheByteBudget);
    const QUrl firstDisplayedUrl = indexedImageUrl(0);
    const QUrl targetUrl = indexedImageUrl(1);
    const QUrl latestDisplayedUrl = indexedImageUrl(2);
    const kiriview::DisplayedImageLocation targetLocation = displayedLocation(targetUrl);

    controller.startWindowLoads(loadWindow(firstDisplayedUrl, { firstDisplayedUrl, targetUrl }));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(QByteArrayLiteral("first snapshot"));
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);

    controller.retireBackgroundLoad(targetLocation);
    QVERIFY(workerScheduler.canceled(1));
    controller.startWindowLoads(
        loadWindow(latestDisplayedUrl, { latestDisplayedUrl, targetUrl }, 8));

    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    workerScheduler.retire(1);

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, targetUrl);
    QCoreApplication::processEvents();
    QCOMPARE(dataLoader.loadCount(), std::size_t(2));
}

void TestPredecodeLoadController::completedDecodeFailureIsNotRetriedAfterPhysicalRetirement()
{
    ManualImageDataLoader dataLoader;
    RetirementControlledWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies decodeDependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoderRejectingBadData());
    decodeDependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::PredecodeLoadController controller(
        this, std::move(decodeDependencies), testCacheByteBudget);
    const QUrl displayedUrl = indexedImageUrl(0);
    const QUrl failingUrl = indexedImageUrl(1);

    controller.startWindowLoads(loadWindow(displayedUrl, { displayedUrl, failingUrl }));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(QByteArrayLiteral("bad"));
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));

    workerScheduler.runWork(1);
    workerScheduler.complete(1);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    workerScheduler.retire(1);
    QCoreApplication::processEvents();
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QVERIFY(!controller.findPredecodedImage(displayedLocation(failingUrl)).has_value());
}

void TestPredecodeLoadController::foregroundOwnerPrecedesWaitingSpeculativeOwnerAfterRetirement()
{
    // Each synthetic byte represents one MiB of policy capacity; no corresponding allocation
    // occurs.
    constexpr qsizetype syntheticPeakByteCount = 610;
    auto workspaceBudget = std::make_shared<kiriview::ImageDecodeWorkspaceBudget>(2048, 1024);
    ManualImageDataLoader speculativeLoader;
    RetirementControlledWorkerScheduler speculativeScheduler;
    kiriview::ImageDecodeDependencies speculativeDependencies
        = imageDecodeDependenciesFor(speculativeLoader, staticImageDataDecoder());
    speculativeDependencies.dataPlanner = syntheticPeakPlanner(syntheticPeakByteCount);
    speculativeDependencies.workerScheduler = speculativeScheduler.scheduler();
    speculativeDependencies.workspaceBudget = workspaceBudget;
    kiriview::PredecodeLoadController controller(
        this, std::move(speculativeDependencies), testCacheByteBudget);

    const QUrl displayedUrl = indexedImageUrl(0);
    const std::vector<QUrl> speculativeUrls {
        indexedImageUrl(1),
        indexedImageUrl(2),
        indexedImageUrl(3),
        indexedImageUrl(4),
    };
    kiriview::PredecodeLoadWindow window = loadWindow(displayedUrl,
        { displayedUrl, speculativeUrls[0], speculativeUrls[1], speculativeUrls[2],
            speculativeUrls[3] });
    window.parallelLimit = 4;
    controller.startWindowLoads(std::move(window));
    QCOMPARE(speculativeLoader.loadCount(), std::size_t(4));

    for (const QUrl& url : speculativeUrls) {
        QVERIFY(speculativeLoader.finishOldestActiveLoadForUrl(url, QByteArrayLiteral("image")));
    }
    QCOMPARE(speculativeScheduler.scheduleCount(), std::size_t(4));
    for (std::size_t index = 0; index < 4; ++index) {
        speculativeScheduler.runWork(index);
        speculativeScheduler.finish(index);
    }
    QTRY_COMPARE(speculativeScheduler.scheduleCount(), std::size_t(7));
    QCOMPARE(workspaceBudget->reservedByteCount(), qsizetype(1830));

    ManualImageDataLoader foregroundLoader;
    RetirementControlledWorkerScheduler foregroundScheduler;
    kiriview::ImageDecodeDependencies foregroundDependencies
        = imageDecodeDependenciesFor(foregroundLoader, staticImageDataDecoder());
    foregroundDependencies.dataPlanner = syntheticPeakPlanner(syntheticPeakByteCount);
    foregroundDependencies.workerScheduler = foregroundScheduler.scheduler();
    foregroundDependencies.workspaceBudget = workspaceBudget;
    int foregroundResults = 0;
    int foregroundLoadErrors = 0;
    kiriview::ImageDecodeJob foregroundJob(nullptr, std::move(foregroundDependencies),
        {
            [&foregroundResults](kiriview::ImageDecodeRequest, kiriview::DecodedImageResult) {
                ++foregroundResults;
            },
            [&foregroundLoadErrors](const kiriview::ImageDecodeRequest&,
                kiriview::ImageDataLoadError) { ++foregroundLoadErrors; },
            {},
            {},
        });
    foregroundJob.start(kiriview::ImageDecodeRequest::fromUrl(99, indexedImageUrl(99)),
        std::nullopt, kiriview::ImageDecodeWorkspacePriority::Interactive);
    foregroundLoader.finishFrontLoad(QByteArrayLiteral("foreground"));
    QCOMPARE(foregroundScheduler.scheduleCount(), std::size_t(1));
    foregroundScheduler.runWork(0);
    foregroundScheduler.finish(0);
    QCoreApplication::processEvents();

    QCOMPARE(foregroundScheduler.scheduleCount(), std::size_t(1));
    QCOMPARE(speculativeScheduler.scheduleCount(), std::size_t(7));
    QCOMPARE(foregroundResults, 0);
    QCOMPARE(foregroundLoadErrors, 0);

    controller.retireBackgroundLoad(displayedLocation(speculativeUrls.front()));
    QVERIFY(speculativeScheduler.canceled(4));
    speculativeScheduler.retire(4);

    QTRY_COMPARE(foregroundScheduler.scheduleCount(), std::size_t(2));
    QCOMPARE(speculativeScheduler.scheduleCount(), std::size_t(7));
    QCOMPARE(workspaceBudget->reservedByteCount(), qsizetype(1830));
    QCOMPARE(foregroundResults, 0);
    QCOMPARE(foregroundLoadErrors, 0);

    foregroundJob.cancel();
    foregroundScheduler.retire(1);
    controller.cancelBackgroundWork();
    speculativeScheduler.retire(5);
    speculativeScheduler.retire(6);
    QTRY_COMPARE(workspaceBudget->reservedByteCount(), qsizetype(0));
}

void TestPredecodeLoadController::cancelBackgroundWorkSuppressesStaleDecode()
{
    ManualImageDataLoader dataLoader;
    ManualImageWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies decodeDependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    decodeDependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::PredecodeLoadController controller(
        this, std::move(decodeDependencies), testCacheByteBudget);
    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);

    controller.startWindowLoads(loadWindow(displayedUrl, { displayedUrl, nextUrl }));
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    controller.cancelBackgroundWork();
    QVERIFY(dataLoader.frontLoad().canceled);

    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("stale"));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(0));
    QVERIFY(!controller.findPredecodedImage(displayedLocation(nextUrl)).has_value());
    QVERIFY(controller.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
}

QTEST_GUILESS_MAIN(TestPredecodeLoadController)

#include "tst_predecodeloadcontroller.moc"
