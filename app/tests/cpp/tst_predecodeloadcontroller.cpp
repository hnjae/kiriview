// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "predecode/predecodeloadcontroller.h"

#include <QObject>
#include <QSize>
#include <QTest>
#include <QUrl>
#include <cstddef>
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
using kiriview::TestSupport::testImage;

constexpr qsizetype testCacheByteBudget = 1024 * 1024;

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
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QVERIFY(controller.findPredecodedImage(displayedLocation(staleNextUrl)).has_value());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, nextUrl);

    dataLoader.finishBackLoad(QByteArrayLiteral("next"));
    QCOMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);
    workerScheduler.finish(1);
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
