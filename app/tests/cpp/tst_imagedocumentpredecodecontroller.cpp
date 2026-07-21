// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentpredecodecontroller.h"
#include "document/imagedocumentstate.h"
#include "image_test_support.h"
#include "navigation/imagedocumentpagecandidaterepository.h"

#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QTest>
#include <QUrl>
#include <memory>
#include <optional>
#include <utility>

namespace {
using kiriview::TestSupport::imageDecodeDependenciesFor;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::imagesDirectoryUrl;
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::ManualImageDataLoader;
using kiriview::TestSupport::ManualPowerSaverMonitor;
using kiriview::TestSupport::ManualTimerScheduler;
using kiriview::TestSupport::powerSaverProviderFor;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::staticImageDataDecoder;
using kiriview::TestSupport::testImage;

using FakeCandidateProvider = kiriview::TestSupport::FakeImageDocumentPageCandidateProvider;

constexpr qsizetype testCacheByteBudget = 1024 * 1024;

kiriview::ImageWorkerScheduler immediateWorkerScheduler()
{
    return kiriview::ImageWorkerScheduler([](QObject*, kiriview::ImageWorkerOperation work,
                                              kiriview::ImageWorkerCompletion completion) {
        work();
        completion();
        return kiriview::ImageWorkerTask {};
    });
}

kiriview::StaticDisplayImagePayload displayTestImagePayload(
    const QImage& image, bool firstDisplay = false)
{
    const kiriview::DisplayImageQuality quality = firstDisplay
        ? kiriview::DisplayImageQuality::FirstDisplay
        : kiriview::DisplayImageQuality::Exact;
    return staticDisplayTestImagePayload(image, image, quality);
}

kiriview::DisplayedPredecodeImage displayedPredecodeImage(
    const kiriview::DisplayedImageLocation& location, kiriview::StaticDisplayImagePayload payload,
    bool cacheable = true)
{
    return { location, cacheable, std::move(payload), {} };
}

kiriview::ImageFirstDisplayDecodeContext firstDisplayContext() { return { QSize(640, 480) }; }

kiriview::ImageDocumentPageCandidateListSnapshot pageCandidateListSnapshot(
    kiriview::ImageDocumentPageCandidateListSource source,
    kiriview::ImageDocumentPageCandidateRows candidates)
{
    kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.revision = 1;
    snapshot.candidates
        = std::make_shared<const kiriview::ImageDocumentPageCandidateRows>(std::move(candidates));
    snapshot.known = true;
    return snapshot;
}

kiriview::ImageDocumentPredecodeController::EnsurePageCandidateSnapshotCallback
candidateSnapshotOwner(QObject* receiver, kiriview::ImageDocumentPageCandidateProvider provider)
{
    auto repository
        = std::make_shared<kiriview::ImageDocumentPageCandidateRepository>(std::move(provider));
    auto jobs = std::make_shared<std::vector<kiriview::ImageIoJob>>();
    return [receiver, repository = std::move(repository), jobs = std::move(jobs)](
               kiriview::ImageDocumentPageCandidateListContext context,
               kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
        const kiriview::ImageDocumentPageCandidateListSource source = context.source();
        auto sharedCallback
            = std::make_shared<kiriview::ImageDocumentPageCandidateListSnapshotCallback>(
                std::move(callback));
        jobs->push_back(repository->loadImages(
            receiver, context,
            [source, sharedCallback](kiriview::ImageDocumentPageCandidateRows candidates) mutable {
                (*sharedCallback)(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                    pageCandidateListSnapshot(source, std::move(candidates)), true, {} });
            },
            [sharedCallback](const QString& errorString) mutable {
                (*sharedCallback)(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                    {}, false, errorString });
            }));
    };
}
}

class TestImageDocumentPredecodeController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scheduleAdjacentImagePredecodeUsesPresentationSnapshot();
    void scheduleAdjacentImagePredecodeUsesCandidateSnapshotCallback();
    void selectedImageNavigationTargetPredecodeLoadsSelectedTargetImmediately();
    void selectedVideoNavigationTargetDoesNotStartPredecode();
    void scheduleAdjacentImagePredecodeWithoutSnapshotCancelsActivePredecode();
    void powerSaverSuppressesBackgroundPredecodeAndReschedulesWhenDisabled();
};

void TestImageDocumentPredecodeController::scheduleAdjacentImagePredecodeUsesPresentationSnapshot()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDocumentPredecodeController controller(
        this, state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget, {},
        candidateSnapshotOwner(this, candidateProvider.provider()));

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    candidateProvider.setDirectoryImages(imagesDirectoryUrl(),
        {
            imageDocumentPageCandidate(displayedUrl),
            imageDocumentPageCandidate(nextUrl),
        });

    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage(QSize(10, 8)), true));

    controller.scheduleAdjacentImagePredecode();

    const std::optional<kiriview::PredecodedImage> displayed
        = controller.findPredecodedImage(displayedUrl);
    QVERIFY(displayed.has_value());
    QCOMPARE(displayed->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QCOMPARE(dataLoader.frontLoad().firstDisplay.logicalViewportSize, QSize(640, 480));
}

void TestImageDocumentPredecodeController::
    scheduleAdjacentImagePredecodeUsesCandidateSnapshotCallback()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(imagesDirectoryUrl(),
            imagesDirectoryUrl(), kiriview::OpenedCollectionScopeKind::Directory);
    const QUrl previousUrl = indexedImageUrl(0);
    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    candidateProvider.setOpenedCollectionCandidateError(
        directoryCollection.rootUrl(), QStringLiteral("unexpected listing"));
    kiriview::ImageDocumentPredecodeController controller(
        this, state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget,
        []() { return 2; },
        [directoryCollection, previousUrl, displayedUrl, nextUrl](
            auto, kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
            callback(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                pageCandidateListSnapshot(
                    kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                        directoryCollection),
                    kiriview::ImageDocumentPageCandidateRows {
                        imageDocumentPageCandidate(previousUrl),
                        imageDocumentPageCandidate(displayedUrl),
                        imageDocumentPageCandidate(nextUrl),
                    }),
                true,
                {},
            });
        });

    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
        displayedUrl, directoryCollection));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()), false);

    controller.scheduleAdjacentImagePredecode();

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QCOMPARE(dataLoader.backLoad().url, previousUrl);
}

void TestImageDocumentPredecodeController::
    selectedImageNavigationTargetPredecodeLoadsSelectedTargetImmediately()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDocumentPredecodeController controller(
        this, state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget, {},
        candidateSnapshotOwner(this, candidateProvider.provider()));

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl oldNextUrl = indexedImageUrl(2);
    const QUrl targetUrl = indexedImageUrl(3);
    const QUrl nextTargetUrl = indexedImageUrl(4);
    candidateProvider.setDirectoryImages(imagesDirectoryUrl(),
        {
            imageDocumentPageCandidate(displayedUrl),
            imageDocumentPageCandidate(oldNextUrl),
            imageDocumentPageCandidate(targetUrl),
            imageDocumentPageCandidate(nextTargetUrl),
        });

    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()));

    controller.scheduleImageNavigationTargetPredecode(
        kiriview::ImageDocumentPageTarget { targetUrl }, 2);

    QVERIFY(controller.findPredecodedImage(displayedUrl).has_value());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, targetUrl);
}

void TestImageDocumentPredecodeController::selectedVideoNavigationTargetDoesNotStartPredecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDocumentPredecodeController controller(
        this, state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget);

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl videoUrl = QUrl::fromLocalFile(QStringLiteral("/images/02.mp4"));
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()));

    controller.scheduleImageNavigationTargetPredecode(
        kiriview::ImageDocumentPageTarget {
            videoUrl,
            kiriview::ImageDocumentPageKind::Video,
        },
        1);

    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
}

void TestImageDocumentPredecodeController::
    scheduleAdjacentImagePredecodeWithoutSnapshotCancelsActivePredecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDocumentPredecodeController controller(
        this, state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget, {},
        candidateSnapshotOwner(this, candidateProvider.provider()));

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    candidateProvider.setDirectoryImages(imagesDirectoryUrl(),
        {
            imageDocumentPageCandidate(displayedUrl),
            imageDocumentPageCandidate(nextUrl),
        });

    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()), false);
    controller.scheduleAdjacentImagePredecode();
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));

    primary.reset();
    controller.scheduleAdjacentImagePredecode();

    QVERIFY(dataLoader.frontLoad().canceled);
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    QVERIFY(!controller.findPredecodedImage(nextUrl).has_value());
}

void TestImageDocumentPredecodeController::
    powerSaverSuppressesBackgroundPredecodeAndReschedulesWhenDisabled()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    ManualPowerSaverMonitor* powerSaverMonitor = nullptr;
    ManualTimerScheduler timerScheduler;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDecodeDependencies decodeDependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    decodeDependencies.workerScheduler = immediateWorkerScheduler();
    kiriview::ImageDocumentPredecodeController controller(
        this, state, [&primary]() { return primary; }, firstDisplayContext,
        std::move(decodeDependencies), testCacheByteBudget, {},
        candidateSnapshotOwner(this, candidateProvider.provider()),
        powerSaverProviderFor(powerSaverMonitor, true), true, timerScheduler.scheduler(),
        []() { return 4; });
    QVERIFY(powerSaverMonitor != nullptr);

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    candidateProvider.setDirectoryImages(imagesDirectoryUrl(),
        {
            imageDocumentPageCandidate(displayedUrl),
            imageDocumentPageCandidate(nextUrl),
        });

    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()));

    timerScheduler.advanceTo(1000);
    controller.scheduleAdjacentImagePredecode();

    QVERIFY(controller.findPredecodedImage(displayedUrl).has_value());
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));

    timerScheduler.advanceTo(1200);
    powerSaverMonitor->setPowerSaverEnabled(false);
    QVERIFY(timerScheduler.timerAt(0).active());
    timerScheduler.timerAt(0).fire();
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);

    powerSaverMonitor->setPowerSaverEnabled(true);
    QVERIFY(dataLoader.frontLoad().canceled);
    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("stale"));
    QVERIFY(!controller.findPredecodedImage(nextUrl).has_value());
    QVERIFY(controller.findPredecodedImage(displayedUrl).has_value());
}

QTEST_GUILESS_MAIN(TestImageDocumentPredecodeController)

#include "tst_imagedocumentpredecodecontroller.moc"
