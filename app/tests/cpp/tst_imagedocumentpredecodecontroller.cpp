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
#include <type_traits>
#include <utility>
#include <vector>

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

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url)
{
    return kiriview::DisplayedImageLocation::fromUrl(url);
}

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
    void selectedImageNavigationTargetRemainsForegroundOwned();
    void selectedImageNavigationTargetPreservesDirectOwnerScopeForAdjacentWork();
    void selectedImageNavigationTargetRetiresMatchingWorkBeforeCandidateSnapshot();
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
        state, [&primary]() { return primary; }, firstDisplayContext,
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
        = controller.findPredecodedImage(displayedLocation(displayedUrl));
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
        state, [&primary]() { return primary; }, firstDisplayContext,
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

void TestImageDocumentPredecodeController::selectedImageNavigationTargetRemainsForegroundOwned()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDocumentPredecodeController controller(
        state, [&primary]() { return primary; }, firstDisplayContext,
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

    QVERIFY(controller.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextTargetUrl);
    QVERIFY(!controller.findPredecodedImage(displayedLocation(targetUrl)).has_value());
}

void TestImageDocumentPredecodeController::
    selectedImageNavigationTargetPreservesDirectOwnerScopeForAdjacentWork()
{
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    const QUrl requestedDisplayedUrl
        = QUrl::fromLocalFile(QStringLiteral("/portal/document/current.png"));
    const QUrl navigationDisplayedUrl
        = QUrl::fromLocalFile(QStringLiteral("/resolved/owner/current.png"));
    const QUrl ownerParentUrl = QUrl::fromLocalFile(QStringLiteral("/resolved/owner/"));
    const QUrl targetUrl = QUrl::fromLocalFile(QStringLiteral("/resolved/owner/target.png"));
    const QUrl adjacentUrl = QUrl::fromLocalFile(QStringLiteral("/resolved/owner/adjacent.png"));
    kiriview::ImageDocumentPageCandidateListContext acceptedContext
        = kiriview::ImageDocumentPageCandidateListContext::forDirectory(targetUrl, ownerParentUrl);
    kiriview::ImageDocumentPredecodeController controller(
        state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget, {},
        [&acceptedContext, targetUrl, adjacentUrl](
            kiriview::ImageDocumentPageCandidateListContext context,
            kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
            acceptedContext = context;
            callback(kiriview::ImageDocumentPageCandidateListSnapshotResult {
                pageCandidateListSnapshot(context.source(),
                    {
                        imageDocumentPageCandidate(targetUrl),
                        imageDocumentPageCandidate(adjacentUrl),
                    }),
                true,
                {},
            });
        });

    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromResolvedSource(
        kiriview::ResolvedNavigationSource(requestedDisplayedUrl, {}, navigationDisplayedUrl)));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()));

    controller.scheduleImageNavigationTargetPredecode(
        kiriview::ImageDocumentPageTarget { targetUrl }, 0);

    QCOMPARE(acceptedContext.currentUrl(), targetUrl);
    QVERIFY(acceptedContext.visit([&ownerParentUrl](const auto& source) {
        using Source = std::decay_t<decltype(source)>;
        if constexpr (std::is_same_v<Source,
                          kiriview::ImageDocumentPageCandidateListSource::Directory>) {
            return kiriview::sameNormalizedUrl(source.directoryUrl, ownerParentUrl);
        }
        return false;
    }));
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, adjacentUrl);
    dataLoader.finishFrontLoad(QByteArrayLiteral("adjacent"));

    const std::optional<kiriview::DirectMediaPageScopeIdentity> adjacentIdentity
        = kiriview::directMediaPageScopeIdentityForOwnerCandidate(
            adjacentUrl, kiriview::sourceKeyForUrl(ownerParentUrl));
    QVERIFY(adjacentIdentity.has_value());
    const kiriview::DisplayedImageLocation scopedAdjacent
        = kiriview::DisplayedImageLocation::fromDirectMediaPageScope(
            adjacentUrl, *adjacentIdentity);
    QTRY_VERIFY(controller.findPredecodedImage(scopedAdjacent).has_value());
    QVERIFY(!controller
            .findPredecodedImage(kiriview::DisplayedImageLocation::fromResolvedSource(
                kiriview::ResolvedNavigationSource(adjacentUrl, {},
                    QUrl::fromLocalFile(QStringLiteral("/resolved/other/adjacent.png")))))
            .has_value());
    const std::optional<kiriview::DirectMediaPageScopeIdentity> targetIdentity
        = kiriview::directMediaPageScopeIdentityForOwnerCandidate(
            targetUrl, kiriview::sourceKeyForUrl(ownerParentUrl));
    QVERIFY(targetIdentity.has_value());
    QVERIFY(!controller
            .findPredecodedImage(kiriview::DisplayedImageLocation::fromDirectMediaPageScope(
                targetUrl, *targetIdentity))
            .has_value());
}

void TestImageDocumentPredecodeController::
    selectedImageNavigationTargetRetiresMatchingWorkBeforeCandidateSnapshot()
{
    ManualImageDataLoader dataLoader;
    ManualTimerScheduler timerScheduler;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    std::vector<kiriview::ImageDocumentPageCandidateListContext> requestedContexts;
    std::vector<kiriview::ImageDocumentPageCandidateListSnapshotCallback> snapshotCallbacks;
    kiriview::ImageDocumentPredecodeController controller(
        state, [&primary]() { return primary; }, firstDisplayContext,
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()), testCacheByteBudget, {},
        [&requestedContexts, &snapshotCallbacks](
            kiriview::ImageDocumentPageCandidateListContext context,
            kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
            requestedContexts.push_back(std::move(context));
            snapshotCallbacks.push_back(std::move(callback));
        },
        {}, true, timerScheduler.scheduler());

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl targetUrl = indexedImageUrl(2);
    const QUrl adjacentUrl = indexedImageUrl(3);
    const kiriview::ImageDocumentPageCandidateRows candidates {
        imageDocumentPageCandidate(displayedUrl),
        imageDocumentPageCandidate(targetUrl),
        imageDocumentPageCandidate(adjacentUrl),
    };
    state.setDisplayedImageLocation(kiriview::DisplayedImageLocation::fromUrl(displayedUrl));
    primary = displayedPredecodeImage(
        state.displayedImageLocation(), displayTestImagePayload(testImage()));

    timerScheduler.advanceTo(1000);
    controller.scheduleAdjacentImagePredecode();
    QCOMPARE(snapshotCallbacks.size(), std::size_t(1));
    snapshotCallbacks.front()(kiriview::ImageDocumentPageCandidateListSnapshotResult {
        pageCandidateListSnapshot(requestedContexts.front().source(), candidates),
        true,
        {},
    });
    QVERIFY(timerScheduler.timerAt(0).active());
    timerScheduler.timerAt(0).fire();
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, targetUrl);

    controller.scheduleImageNavigationTargetPredecode(
        kiriview::ImageDocumentPageTarget { targetUrl }, 1);

    QCOMPARE(snapshotCallbacks.size(), std::size_t(2));
    QVERIFY(dataLoader.frontLoad().canceled);
    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("stale target"));
    QVERIFY(!controller.findPredecodedImage(displayedLocation(targetUrl)).has_value());

    snapshotCallbacks.back()(kiriview::ImageDocumentPageCandidateListSnapshotResult {
        pageCandidateListSnapshot(requestedContexts.back().source(), candidates),
        true,
        {},
    });
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, adjacentUrl);
}

void TestImageDocumentPredecodeController::selectedVideoNavigationTargetDoesNotStartPredecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImageDocumentState state;
    std::optional<kiriview::DisplayedPredecodeImage> primary;
    kiriview::ImageDocumentPredecodeController controller(
        state, [&primary]() { return primary; }, firstDisplayContext,
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
        state, [&primary]() { return primary; }, firstDisplayContext,
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
    QVERIFY(!controller.findPredecodedImage(displayedLocation(nextUrl)).has_value());
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
        state, [&primary]() { return primary; }, firstDisplayContext, std::move(decodeDependencies),
        testCacheByteBudget, {}, candidateSnapshotOwner(this, candidateProvider.provider()),
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

    QVERIFY(controller.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
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
    QVERIFY(!controller.findPredecodedImage(displayedLocation(nextUrl)).has_value());
    QVERIFY(controller.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
}

QTEST_GUILESS_MAIN(TestImageDocumentPredecodeController)

#include "tst_imagedocumentpredecodecontroller.moc"
