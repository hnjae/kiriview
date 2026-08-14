// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "decoding/kiriimagedecoder.h"
#include "image_test_support.h"
#include "location/imagedocumentlocation.h"
#include "navigation/imagedocumentpagecandidatelistsource.h"
#include "predecode/imagepredecodecoordinator.h"

#include <QCoreApplication>
#include <QFile>
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
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::imageDecodeDependenciesFor;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::imagesDirectoryUrl;
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::ManualImageDataLoader;
using kiriview::TestSupport::ManualPowerSaverMonitor;
using kiriview::TestSupport::ManualTimerScheduler;
using kiriview::TestSupport::powerSaverProviderFor;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::staticImageDataDecoder;
using kiriview::TestSupport::testImage;

using FakeCandidateProvider = kiriview::TestSupport::FakeImageDocumentPageCandidateProvider;

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
        const auto& schedule = m_schedules.at(index);
        schedule->taskCompletion.claimAndRun([&]() {
            if (schedule->completion) {
                schedule->completion();
            }
        });
        schedule->taskCompletion.retire();
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

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url,
    const kiriview::OpenedCollectionScopeLocation& openedCollectionScope
    = kiriview::OpenedCollectionScopeLocation::none())
{
    return kiriview::DisplayedImageLocation::fromUrl(url, openedCollectionScope);
}

kiriview::PowerSaverProvider noOpPowerSaverProvider()
{
    return kiriview::PowerSaverProvider {
        [](kiriview::PowerSaverChangedCallback) {
            return std::unique_ptr<kiriview::PowerSaverStateMonitor>();
        },
    };
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

kiriview::ImageDataDecoder defaultImageDataDecoder()
{
    return [](const QByteArray& data, const kiriview::ImageDecodeRequest& request) {
        return kiriview::decodeImageData(data, request);
    };
}

kiriview::ImagePredecodeCoordinator createCoordinator(FakeCandidateProvider& candidateProvider,
    ManualImageDataLoader& dataLoader, kiriview::PowerSaverProvider powerSaverProvider,
    kiriview::TimerScheduler timerScheduler = {},
    kiriview::PredecodeThreadCountProvider threadCountProvider = {})
{
    Q_UNUSED(candidateProvider);
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    dependencies.workerScheduler = immediateWorkerScheduler();
    return kiriview::ImagePredecodeCoordinator(std::move(dependencies),
        std::move(powerSaverProvider), testCacheByteBudget, std::move(timerScheduler),
        std::move(threadCountProvider));
}

kiriview::ImagePredecodeCoordinator createCoordinator(FakeCandidateProvider& candidateProvider,
    ManualImageDataLoader& dataLoader, kiriview::ImageDataDecoder dataDecoder,
    kiriview::TimerScheduler timerScheduler = {},
    kiriview::PredecodeThreadCountProvider threadCountProvider = {})
{
    Q_UNUSED(candidateProvider);
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, std::move(dataDecoder));
    dependencies.workerScheduler = immediateWorkerScheduler();
    return kiriview::ImagePredecodeCoordinator(std::move(dependencies), noOpPowerSaverProvider(),
        testCacheByteBudget, std::move(timerScheduler), std::move(threadCountProvider));
}

kiriview::ImagePredecodeCoordinator createCoordinator(
    FakeCandidateProvider& candidateProvider, ManualImageDataLoader& dataLoader)
{
    return createCoordinator(candidateProvider, dataLoader, noOpPowerSaverProvider());
}

std::vector<kiriview::ImageDocumentPageCandidate> imageDocumentPageCandidates(int count)
{
    std::vector<kiriview::ImageDocumentPageCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        candidates.push_back(imageDocumentPageCandidate(indexedImageUrl(index)));
    }
    return candidates;
}

kiriview::ImageDocumentPageCandidateListSnapshot pageCandidateListSnapshot(
    kiriview::ImageDocumentPageCandidateListSource source,
    kiriview::ImageDocumentPageCandidateRows candidates, bool known = true, quint64 revision = 1)
{
    kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.revision = revision;
    snapshot.candidates
        = std::make_shared<const kiriview::ImageDocumentPageCandidateRows>(std::move(candidates));
    snapshot.known = known;
    return snapshot;
}

kiriview::StaticDisplayImagePayload displayTestImagePayload(
    const QImage& image, bool firstDisplay = false)
{
    const kiriview::DisplayImageQuality quality = firstDisplay
        ? kiriview::DisplayImageQuality::FirstDisplay
        : kiriview::DisplayImageQuality::Exact;
    return staticDisplayTestImagePayload(image, image, quality);
}

kiriview::ImagePredecodeCoordinator::Context predecodeContext(
    kiriview::DisplayedPredecodeImage primaryImage,
    std::optional<kiriview::DisplayedPredecodeImage> secondaryImage = std::nullopt,
    kiriview::ImageFirstDisplayDecodeContext firstDisplayContext = {}, int pageIndex = -1)
{
    const kiriview::DisplayedImageLocation currentLocation = primaryImage.location;
    std::vector<kiriview::DisplayedPredecodeImage> displayedImages;
    displayedImages.push_back(std::move(primaryImage));
    if (secondaryImage.has_value()) {
        displayedImages.push_back(std::move(*secondaryImage));
    }

    return kiriview::ImagePredecodeCoordinator::Context {
        currentLocation,
        std::move(displayedImages),
        firstDisplayContext,
        pageIndex,
        {},
    };
}

kiriview::ImagePredecodeCoordinator::Context withCandidateSnapshot(
    kiriview::ImagePredecodeCoordinator::Context context,
    kiriview::ImageDocumentPageCandidateListSource source,
    kiriview::ImageDocumentPageCandidateRows candidates)
{
    context.candidateSnapshot = pageCandidateListSnapshot(std::move(source), std::move(candidates));
    return context;
}

QString fixturePath(const QString& fileName)
{
    return QStringLiteral(KIRIVIEW_TEST_SOURCE_DIR "/../fixtures/") + fileName;
}

QByteArray fixtureData(const QString& fileName)
{
    QFile file(fixturePath(fileName));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}
}

class TestImagePredecodeCoordinator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void scheduleCachesDisplayedImageAndPredecodesWindow();
    void scheduleCachesVisibleSpreadPagesAndSkipsSecondaryPredecode();
    void scheduleRejectsInvalidDisplayedContext();
    void selectedTargetRetiresMatchingBackgroundWork();
    void archivePredecodeKeepsOpenedCollectionScopeContext();
    void regularPredecodeWindowKeepsTwoPreviousAndTwoNextPages();
    void directoryCollectionStartsTwoBackgroundDecodes();
    void openedCollectionSnapshotPlansWindowWithoutListing();
    void staleOpenedCollectionSnapshotDoesNotCreatePrivateCandidateAuthority();
    void missingCandidateSnapshotStartsEmptyFallbackWindow();
    void archiveThreadCountProviderControlsParallelLoadLimit();
    void animatedBackgroundDecodeIsNotCachedAsStaticPredecodedImage_data();
    void animatedBackgroundDecodeIsNotCachedAsStaticPredecodedImage();
    void sameScopeGenerationChangeRetainsActiveDecodeCompletion();
    void candidateSnapshotIdentityRemainsUntilConflictingWorkPhysicallyRetires();
    void rapidNavigationDebouncesSkippedPagePredecode();
    void powerSaverMonitorSuppressesAndReschedulesPredecode();
    void cancelSuppressesPendingDecode();
};

void TestImagePredecodeCoordinator::scheduleCachesDisplayedImageAndPredecodesWindow()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const QUrl previousUrl = indexedImageUrl(0);
    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    const QImage displayedImage = testImage();
    coordinator.schedule(withCandidateSnapshot(
        predecodeContext(
            kiriview::DisplayedPredecodeImage {
                kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
                true,
                displayTestImagePayload(displayedImage, true),
            },
            std::nullopt, kiriview::ImageFirstDisplayDecodeContext { QSize(640, 480) }),
        kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
        {
            imageDocumentPageCandidate(previousUrl),
            imageDocumentPageCandidate(displayedUrl),
            imageDocumentPageCandidate(nextUrl),
        }));

    const std::optional<kiriview::PredecodedImage> displayed
        = coordinator.findPredecodedImage(displayedLocation(displayedUrl));
    QVERIFY(displayed.has_value());
    QCOMPARE(displayed->location.imageUrl(), displayedUrl);
    QCOMPARE(displayed->displayImage.quality, kiriview::DisplayImageQuality::FirstDisplay);

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QCOMPARE(dataLoader.frontLoad().firstDisplay.logicalViewportSize, QSize(640, 480));
    dataLoader.finishFrontLoad(QByteArrayLiteral("next"));

    QTRY_VERIFY(coordinator.findPredecodedImage(displayedLocation(nextUrl)).has_value());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, previousUrl);
}

void TestImagePredecodeCoordinator::scheduleCachesVisibleSpreadPagesAndSkipsSecondaryPredecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const QUrl primaryUrl = indexedImageUrl(0);
    const QUrl secondaryUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(
                                  kiriview::DisplayedPredecodeImage {
                                      kiriview::DisplayedImageLocation::fromUrl(primaryUrl),
                                      true,
                                      displayTestImagePayload(testImage(), true),
                                  },
                                  std::make_optional(kiriview::DisplayedPredecodeImage {
                                      kiriview::DisplayedImageLocation::fromUrl(secondaryUrl),
                                      true,
                                      displayTestImagePayload(testImage(), true),
                                  })),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            {
                imageDocumentPageCandidate(primaryUrl),
                imageDocumentPageCandidate(secondaryUrl),
                imageDocumentPageCandidate(nextUrl),
            }));

    const std::optional<kiriview::PredecodedImage> primary
        = coordinator.findPredecodedImage(displayedLocation(primaryUrl));
    QVERIFY(primary.has_value());

    const std::optional<kiriview::PredecodedImage> secondary
        = coordinator.findPredecodedImage(displayedLocation(secondaryUrl));
    QVERIFY(secondary.has_value());

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
}

void TestImagePredecodeCoordinator::scheduleRejectsInvalidDisplayedContext()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    coordinator.schedule(kiriview::ImagePredecodeCoordinator::Context {});

    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
}

void TestImagePredecodeCoordinator::selectedTargetRetiresMatchingBackgroundWork()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const QUrl displayedUrl = indexedImageUrl(0);
    const QUrl selectedUrl = indexedImageUrl(1);
    const QUrl adjacentUrl = indexedImageUrl(2);
    const kiriview::ImageDocumentPageCandidateListSource candidateSource
        = kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl());
    const std::vector candidates {
        imageDocumentPageCandidate(displayedUrl),
        imageDocumentPageCandidate(selectedUrl),
        imageDocumentPageCandidate(adjacentUrl),
    };
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
                                  true,
                                  displayTestImagePayload(testImage()),
                              }),
            candidateSource, candidates));
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, selectedUrl);

    kiriview::ImagePredecodeCoordinator::Context selectedContext
        = predecodeContext(kiriview::DisplayedPredecodeImage {
            kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
            true,
            displayTestImagePayload(testImage()),
        });
    selectedContext.currentLocation = kiriview::DisplayedImageLocation::fromUrl(selectedUrl);
    selectedContext.immediate = true;
    coordinator.schedule(
        withCandidateSnapshot(std::move(selectedContext), candidateSource, candidates));

    QVERIFY(dataLoader.frontLoad().canceled);
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, adjacentUrl);

    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("stale target"));
    QCOMPARE(dataLoader.loadCount(), std::size_t(2));
}

void TestImagePredecodeCoordinator::archivePredecodeKeepsOpenedCollectionScopeContext()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> openedCollectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(openedCollectionScope.has_value());
    const QUrl displayedUrl
        = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("01.png"));
    const QUrl nextUrl = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("02.png"));
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                                      displayedUrl, *openedCollectionScope),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                *openedCollectionScope),
            {
                imageDocumentPageCandidate(displayedUrl),
                imageDocumentPageCandidate(nextUrl),
            }));

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QCOMPARE(
        dataLoader.frontLoad().openedCollectionScope.rootUrl(), openedCollectionScope->rootUrl());
    dataLoader.finishFrontLoad(QByteArrayLiteral("next"));

    const kiriview::DisplayedImageLocation nextLocation
        = displayedLocation(nextUrl, *openedCollectionScope);
    QTRY_VERIFY(coordinator.findPredecodedImage(nextLocation).has_value());
    const std::optional<kiriview::PredecodedImage> predecoded
        = coordinator.findPredecodedImage(nextLocation);
    QVERIFY(predecoded.has_value());
    QCOMPARE(
        predecoded->location.openedCollectionScope().rootUrl(), openedCollectionScope->rootUrl());
}

void TestImagePredecodeCoordinator::regularPredecodeWindowKeepsTwoPreviousAndTwoNextPages()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const QUrl displayedUrl = indexedImageUrl(5);
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            imageDocumentPageCandidates(15)));

    const std::vector<QUrl> expectedLoadOrder {
        indexedImageUrl(6),
        indexedImageUrl(4),
        indexedImageUrl(7),
        indexedImageUrl(3),
    };
    std::size_t expectedLoadCount = 0;
    for (const QUrl& expectedUrl : expectedLoadOrder) {
        ++expectedLoadCount;
        QTRY_VERIFY(dataLoader.loadCount() >= expectedLoadCount);
        QCOMPARE(dataLoader.backLoad().url, expectedUrl);
        QVERIFY(dataLoader.finishOldestActiveLoadForUrl(expectedUrl, QByteArrayLiteral("image")));
    }

    QTRY_COMPARE(dataLoader.loadCount(), expectedLoadOrder.size());
    QVERIFY(
        !dataLoader.finishOldestActiveLoadForUrl(indexedImageUrl(8), QByteArrayLiteral("image")));
}

void TestImagePredecodeCoordinator::directoryCollectionStartsTwoBackgroundDecodes()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(imagesDirectoryUrl(),
            imagesDirectoryUrl(), kiriview::OpenedCollectionScopeKind::Directory);
    const QUrl displayedUrl = indexedImageUrl(5);
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                                      displayedUrl, directoryCollection),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                directoryCollection),
            imageDocumentPageCandidates(15)));

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.frontLoad().url, indexedImageUrl(6));
    QCOMPARE(dataLoader.backLoad().url, indexedImageUrl(4));
}

void TestImagePredecodeCoordinator::openedCollectionSnapshotPlansWindowWithoutListing()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(imagesDirectoryUrl(),
            imagesDirectoryUrl(), kiriview::OpenedCollectionScopeKind::Directory);
    const QUrl displayedUrl = indexedImageUrl(5);
    candidateProvider.setOpenedCollectionCandidateError(
        directoryCollection.rootUrl(), QStringLiteral("unexpected listing"));

    kiriview::ImagePredecodeCoordinator::Context context
        = predecodeContext(kiriview::DisplayedPredecodeImage {
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                displayedUrl, directoryCollection),
            true,
            displayTestImagePayload(testImage()),
        });
    context.candidateSnapshot = pageCandidateListSnapshot(
        kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            directoryCollection),
        imageDocumentPageCandidates(15));

    coordinator.schedule(std::move(context));

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.frontLoad().url, indexedImageUrl(6));
    QCOMPARE(dataLoader.backLoad().url, indexedImageUrl(4));
}

void TestImagePredecodeCoordinator::
    staleOpenedCollectionSnapshotDoesNotCreatePrivateCandidateAuthority()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(imagesDirectoryUrl(),
            imagesDirectoryUrl(), kiriview::OpenedCollectionScopeKind::Directory);
    const QUrl staleDirectoryUrl = localUrl(QStringLiteral("/images/stale"));
    const kiriview::OpenedCollectionScopeLocation staleDirectoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(
            staleDirectoryUrl, staleDirectoryUrl, kiriview::OpenedCollectionScopeKind::Directory);
    const QUrl displayedUrl = indexedImageUrl(5);
    candidateProvider.setOpenedCollectionCandidates(
        directoryCollection.rootUrl(), imageDocumentPageCandidates(15));

    kiriview::ImagePredecodeCoordinator::Context context
        = predecodeContext(kiriview::DisplayedPredecodeImage {
            kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                displayedUrl, directoryCollection),
            true,
            displayTestImagePayload(testImage()),
        });
    context.candidateSnapshot = pageCandidateListSnapshot(
        kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
            staleDirectoryCollection),
        imageDocumentPageCandidates(15));

    coordinator.schedule(std::move(context));

    QCOMPARE(
        candidateProvider.openedCollectionCandidateLoadCount(directoryCollection.rootUrl()), 0);
    QCOMPARE(
        candidateProvider.openedCollectionCandidateLoadCount(staleDirectoryCollection.rootUrl()),
        0);
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl, directoryCollection))
            .has_value());
}

void TestImagePredecodeCoordinator::missingCandidateSnapshotStartsEmptyFallbackWindow()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    ManualTimerScheduler timerScheduler;
    kiriview::ImagePredecodeCoordinator coordinator = createCoordinator(
        candidateProvider, dataLoader, noOpPowerSaverProvider(), timerScheduler.scheduler());

    const QUrl displayedUrl = indexedImageUrl(5);
    timerScheduler.advanceTo(1000);
    coordinator.schedule(predecodeContext(kiriview::DisplayedPredecodeImage {
        kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
        true,
        displayTestImagePayload(testImage()),
    }));

    timerScheduler.timerAt(0).fire();

    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
}

void TestImagePredecodeCoordinator::archiveThreadCountProviderControlsParallelLoadLimit()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    ManualTimerScheduler timerScheduler;
    kiriview::ImagePredecodeCoordinator coordinator = createCoordinator(candidateProvider,
        dataLoader, noOpPowerSaverProvider(), timerScheduler.scheduler(), []() { return 8; });

    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> openedCollectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(openedCollectionScope.has_value());
    const QUrl displayedUrl
        = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("05.png"));
    std::vector<kiriview::ImageDocumentPageCandidate> candidates;
    for (int index = 0; index < 15; ++index) {
        candidates.push_back(
            imageDocumentPageCandidate(archivePageUrl(openedCollectionScope->rootUrl(),
                QStringLiteral("%1.png").arg(index, 2, 10, QLatin1Char('0')))));
    }
    timerScheduler.advanceTo(1000);
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
                                      displayedUrl, *openedCollectionScope),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forOpenedCollectionScope(
                *openedCollectionScope),
            std::move(candidates)));

    timerScheduler.timerAt(0).fire();

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(4));
}

void TestImagePredecodeCoordinator::
    animatedBackgroundDecodeIsNotCachedAsStaticPredecodedImage_data()
{
    QTest::addColumn<QString>("fileName");

    QTest::newRow("apng") << QStringLiteral("animated-smoke.apng");
    QTest::newRow("webp") << QStringLiteral("animated-smoke.webp");
    QTest::newRow("jxl") << QStringLiteral("animated-smoke.jxl");
    QTest::newRow("heif-sequence") << QStringLiteral("heif-sequence-alpha.heics");
}

void TestImagePredecodeCoordinator::animatedBackgroundDecodeIsNotCachedAsStaticPredecodedImage()
{
    QFETCH(QString, fileName);

    const QByteArray imageData = fixtureData(fileName);
    QVERIFY2(!imageData.isEmpty(), qPrintable(fixturePath(fileName)));

    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader, defaultImageDataDecoder());

    const QUrl displayedUrl = indexedImageUrl(0);
    const QUrl animatedUrl = indexedImageUrl(1);
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            {
                imageDocumentPageCandidate(displayedUrl),
                imageDocumentPageCandidate(animatedUrl),
            }));

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, animatedUrl);
    dataLoader.finishFrontLoad(imageData);

    QVERIFY(!coordinator.findPredecodedImage(displayedLocation(animatedUrl)).has_value());
}

void TestImagePredecodeCoordinator::sameScopeGenerationChangeRetainsActiveDecodeCompletion()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(0)),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            imageDocumentPageCandidates(5)));
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, indexedImageUrl(1));

    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(2)),
                                  false,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            imageDocumentPageCandidates(5)));

    dataLoader.finishFrontLoad(QByteArrayLiteral("warm"));
    QTRY_VERIFY(coordinator.findPredecodedImage(displayedLocation(indexedImageUrl(1))).has_value());
}

void TestImagePredecodeCoordinator::
    candidateSnapshotIdentityRemainsUntilConflictingWorkPhysicallyRetires()
{
    ManualImageDataLoader dataLoader;
    ManualTimerScheduler timerScheduler;
    RetirementControlledWorkerScheduler workerScheduler;
    kiriview::ImageDecodeDependencies dependencies
        = imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder());
    dependencies.workerScheduler = workerScheduler.scheduler();
    kiriview::ImagePredecodeCoordinator coordinator(std::move(dependencies),
        noOpPowerSaverProvider(), testCacheByteBudget, timerScheduler.scheduler());
    const QUrl displayedUrl = indexedImageUrl(0);
    const QUrl targetUrl = indexedImageUrl(1);
    const kiriview::DisplayedImageLocation targetLocation = displayedLocation(targetUrl);
    const kiriview::ImageDocumentPageCandidateListSource source
        = kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl());
    const std::vector candidates {
        imageDocumentPageCandidate(displayedUrl),
        imageDocumentPageCandidate(targetUrl),
    };
    const auto contextForRevision = [&](quint64 revision) {
        kiriview::ImagePredecodeCoordinator::Context context
            = predecodeContext(kiriview::DisplayedPredecodeImage {
                displayedLocation(displayedUrl),
                true,
                displayTestImagePayload(testImage()),
            });
        context.candidateSnapshot = pageCandidateListSnapshot(source, candidates, true, revision);
        return context;
    };

    timerScheduler.advanceTo(1000);
    coordinator.schedule(contextForRevision(41));
    timerScheduler.timerAt(0).fire();
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
    dataLoader.finishFrontLoad(QByteArrayLiteral("candidate snapshot"));
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(1));
    workerScheduler.runWork(0);
    workerScheduler.finish(0);
    QTRY_COMPARE(workerScheduler.scheduleCount(), std::size_t(2));
    workerScheduler.runWork(1);

    timerScheduler.advanceTo(1200);
    coordinator.schedule(contextForRevision(41));
    timerScheduler.timerAt(0).fire();
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    coordinator.acceptForegroundSelection(targetLocation);
    QVERIFY(workerScheduler.canceled(1));
    timerScheduler.advanceTo(1400);
    coordinator.schedule(contextForRevision(42));
    timerScheduler.timerAt(0).fire();
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));

    workerScheduler.retire(1);

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, targetUrl);
    QCoreApplication::processEvents();
    QCOMPARE(dataLoader.loadCount(), std::size_t(2));
}

void TestImagePredecodeCoordinator::rapidNavigationDebouncesSkippedPagePredecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    ManualTimerScheduler timerScheduler;
    kiriview::ImagePredecodeCoordinator coordinator = createCoordinator(
        candidateProvider, dataLoader, noOpPowerSaverProvider(), timerScheduler.scheduler());

    const auto schedulePage = [&coordinator](int pageIndex) {
        coordinator.schedule(withCandidateSnapshot(
            predecodeContext(
                kiriview::DisplayedPredecodeImage {
                    kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(pageIndex)),
                    false,
                    displayTestImagePayload(testImage()),
                },
                std::nullopt, {}, pageIndex),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            imageDocumentPageCandidates(10)));
    };

    timerScheduler.advanceTo(1000);
    schedulePage(0);
    timerScheduler.advanceTo(1100);
    schedulePage(1);
    timerScheduler.advanceTo(1200);
    schedulePage(2);

    QCOMPARE(dataLoader.loadCount(), std::size_t(0));

    timerScheduler.timerAt(0).fire();
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
    timerScheduler.timerAt(1).fire();

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.backLoad().url, indexedImageUrl(3));
    QVERIFY(
        !dataLoader.finishOldestActiveLoadForUrl(indexedImageUrl(1), QByteArrayLiteral("image")));
}

void TestImagePredecodeCoordinator::powerSaverMonitorSuppressesAndReschedulesPredecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    ManualPowerSaverMonitor* powerSaverMonitor = nullptr;
    ManualTimerScheduler timerScheduler;
    kiriview::ImagePredecodeCoordinator coordinator = createCoordinator(candidateProvider,
        dataLoader, powerSaverProviderFor(powerSaverMonitor, true), timerScheduler.scheduler());
    QVERIFY(powerSaverMonitor != nullptr);
    QVERIFY(coordinator.powerSaverEnabled());

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    timerScheduler.advanceTo(1000);
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
                                  true,
                                  displayTestImagePayload(testImage()),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            {
                imageDocumentPageCandidate(displayedUrl),
                imageDocumentPageCandidate(nextUrl),
            }));

    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));

    timerScheduler.advanceTo(1200);
    powerSaverMonitor->setPowerSaverEnabled(false);
    QVERIFY(!coordinator.powerSaverEnabled());
    QVERIFY(timerScheduler.timerAt(0).active());
    timerScheduler.timerAt(0).fire();
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);

    powerSaverMonitor->setPowerSaverEnabled(true);
    QVERIFY(coordinator.powerSaverEnabled());
    QVERIFY(dataLoader.frontLoad().canceled);
    dataLoader.deliverFrontLoadDataIgnoringCancellation(QByteArrayLiteral("stale"));
    QVERIFY(!coordinator.findPredecodedImage(displayedLocation(nextUrl)).has_value());
    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
}

void TestImagePredecodeCoordinator::cancelSuppressesPendingDecode()
{
    FakeCandidateProvider candidateProvider;
    ManualImageDataLoader dataLoader;
    kiriview::ImagePredecodeCoordinator coordinator
        = createCoordinator(candidateProvider, dataLoader);

    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl nextUrl = indexedImageUrl(2);
    const QImage displayedImage = testImage();
    coordinator.schedule(
        withCandidateSnapshot(predecodeContext(kiriview::DisplayedPredecodeImage {
                                  kiriview::DisplayedImageLocation::fromUrl(displayedUrl),
                                  false,
                                  displayTestImagePayload(displayedImage),
                              }),
            kiriview::ImageDocumentPageCandidateListSource::forDirectory(imagesDirectoryUrl()),
            {
                imageDocumentPageCandidate(displayedUrl),
                imageDocumentPageCandidate(nextUrl),
            }));

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    coordinator.cancel();
    QVERIFY(dataLoader.frontLoad().canceled);

    dataLoader.finishFrontLoad(QByteArrayLiteral("next"));
    QVERIFY(!coordinator.findPredecodedImage(displayedLocation(nextUrl)).has_value());
    QCOMPARE(dataLoader.loadCount(), std::size_t(1));
}

QTEST_GUILESS_MAIN(TestImagePredecodeCoordinator)

#include "tst_imagepredecodecoordinator.moc"
