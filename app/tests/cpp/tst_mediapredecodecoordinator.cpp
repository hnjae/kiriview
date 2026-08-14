// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "predecode/mediapredecodecoordinator.h"

#include <QObject>
#include <QSet>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using kiriview::TestSupport::imageDecodeDependenciesFor;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::ManualImageDataLoader;
using kiriview::TestSupport::ManualPowerSaverMonitor;
using kiriview::TestSupport::ManualTimerScheduler;
using kiriview::TestSupport::powerSaverProviderFor;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::staticImageDataDecoder;
using kiriview::TestSupport::testImage;

constexpr qsizetype testCacheByteBudget = 1024 * 1024;

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url)
{
    return kiriview::DisplayedImageLocation::fromUrl(url);
}

kiriview::DisplayedImageLocation displayedLocation(
    const QUrl& url, const kiriview::DirectMediaScope& ownerScope)
{
    const std::optional<kiriview::DirectMediaPageScopeIdentity> identity
        = kiriview::directMediaPageScopeIdentityForOwnerCandidate(url, ownerScope.parentKey());
    Q_ASSERT(identity.has_value());
    return kiriview::DisplayedImageLocation::fromDirectMediaPageScope(url, *identity);
}

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

std::vector<kiriview::DirectMediaNavigationCandidate> mixedDirectMediaNavigationCandidates()
{
    return {
        directMediaNavigationCandidate(localUrl(QStringLiteral("/media/00.png"))),
        directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.mp4"))),
        directMediaNavigationCandidate(localUrl(QStringLiteral("/media/02.png"))),
        directMediaNavigationCandidate(localUrl(QStringLiteral("/media/03.png"))),
    };
}

kiriview::DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot(
    const kiriview::ResolvedNavigationSource& source,
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
{
    kiriview::DirectMediaNavigationCandidateSnapshot snapshot;
    if (!candidates.empty()) {
        snapshot.source = kiriview::DirectMediaScope::fromSource(source, 4);
    }
    snapshot.revision = 1;
    snapshot.candidates
        = std::make_shared<const std::vector<kiriview::DirectMediaNavigationCandidate>>(
            std::move(candidates));
    snapshot.boundaryState.currentNumber = 1;
    snapshot.boundaryState.count = static_cast<int>(snapshot.candidates->size());
    snapshot.known = true;
    return snapshot;
}

kiriview::DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot(
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
{
    const QUrl sourceUrl = candidates.empty() ? QUrl() : candidates.front().url;
    return directMediaNavigationCandidateSnapshot(
        kiriview::ResolvedNavigationSource(sourceUrl, {}, sourceUrl), std::move(candidates));
}

kiriview::DisplayedPredecodeImage displayedImage(const QUrl& url)
{
    return kiriview::DisplayedPredecodeImage {
        kiriview::DisplayedImageLocation::fromUrl(url),
        true,
        staticDisplayTestImagePayload(testImage()),
    };
}

kiriview::MediaPredecodeCoordinator::Context predecodeContext(const QUrl& currentUrl,
    kiriview::DirectMediaNavigationCandidateSnapshot candidateSnapshot,
    std::vector<kiriview::DisplayedPredecodeImage> displayedImages = {}, bool immediate = false)
{
    return kiriview::MediaPredecodeCoordinator::Context {
        currentUrl,
        candidateSnapshot.source,
        std::move(candidateSnapshot),
        std::move(displayedImages),
        {},
        immediate,
    };
}

kiriview::PowerSaverProvider noOpPowerSaverProvider()
{
    return kiriview::PowerSaverProvider {
        [](kiriview::PowerSaverChangedCallback) {
            return std::unique_ptr<kiriview::PowerSaverStateMonitor>();
        },
    };
}

kiriview::MediaPredecodeCoordinator createCoordinator(ManualImageDataLoader& dataLoader,
    kiriview::PowerSaverProvider powerSaverProvider, kiriview::TimerScheduler timerScheduler = {})
{
    return kiriview::MediaPredecodeCoordinator(kiriview::MediaPredecodeDependencies {
        imageDecodeDependenciesFor(dataLoader, staticImageDataDecoder()),
        std::move(powerSaverProvider),
        testCacheByteBudget,
        std::move(timerScheduler),
    });
}

kiriview::MediaPredecodeCoordinator createCoordinator(ManualImageDataLoader& dataLoader)
{
    return createCoordinator(dataLoader, noOpPowerSaverProvider());
}
}

class TestMediaPredecodeCoordinator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void videoCursorKeepsImageCacheAndLoadsAdjacentImages();
    void differentDirectMediaScopesDoNotShareAdjacentPredecode();
    void powerSaverSuppressesLoadsButRetainsDisplayedImages();
    void powerSaverReschedulesVideoCursorWithoutDisplayedImages();
    void invalidScheduleClearsSuppressedDirectMediaNavigationCandidates();
};

void TestMediaPredecodeCoordinator::videoCursorKeepsImageCacheAndLoadsAdjacentImages()
{
    ManualImageDataLoader dataLoader;
    kiriview::MediaPredecodeCoordinator coordinator = createCoordinator(dataLoader);

    const QUrl displayedUrl = localUrl(QStringLiteral("/media/00.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    const QUrl laterUrl = localUrl(QStringLiteral("/media/03.png"));
    const std::vector candidateRows = mixedDirectMediaNavigationCandidates();
    const kiriview::DirectMediaNavigationCandidateSnapshot displayedCandidates
        = directMediaNavigationCandidateSnapshot(mixedDirectMediaNavigationCandidates());
    const kiriview::DirectMediaNavigationCandidateSnapshot videoCandidates
        = directMediaNavigationCandidateSnapshot(
            kiriview::ResolvedNavigationSource(videoUrl, {}, videoUrl), candidateRows);

    coordinator.schedule(
        predecodeContext(displayedUrl, displayedCandidates, { displayedImage(displayedUrl) }));

    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QVERIFY(dataLoader.finishOldestActiveLoadForUrl(nextUrl, QByteArrayLiteral("next")));
    QTRY_VERIFY(coordinator.findPredecodedImage(displayedLocation(nextUrl)).has_value());

    coordinator.schedule(predecodeContext(videoUrl, videoCandidates));

    QSet<QUrl> loadedUrls;
    for (std::size_t loadCount = 2; loadCount <= 3; ++loadCount) {
        QTRY_COMPARE(dataLoader.loadCount(), loadCount);
        const QUrl loadedUrl = dataLoader.backLoad().url;
        loadedUrls.insert(loadedUrl);
        if (loadCount < 3) {
            QVERIFY(dataLoader.finishOldestActiveLoadForUrl(
                loadedUrl, QByteArrayLiteral("revalidated")));
        }
    }
    QCOMPARE(loadedUrls, QSet<QUrl>({ displayedUrl, laterUrl }));
    QCOMPARE(dataLoader.loadCountForUrl(nextUrl), std::size_t(1));
    QVERIFY(!loadedUrls.contains(videoUrl));
    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
    QVERIFY(coordinator.findPredecodedImage(displayedLocation(nextUrl)).has_value());
}

void TestMediaPredecodeCoordinator::differentDirectMediaScopesDoNotShareAdjacentPredecode()
{
    ManualImageDataLoader dataLoader;
    kiriview::MediaPredecodeCoordinator coordinator = createCoordinator(dataLoader);

    const QUrl requestedCurrentUrl = localUrl(QStringLiteral("/portal/document/current.mp4"));
    const QUrl firstAdjacentImageUrl = localUrl(QStringLiteral("/resolved/first/adjacent.png"));
    const QUrl secondAdjacentImageUrl = localUrl(QStringLiteral("/resolved/second/adjacent.png"));
    const QUrl firstCurrentUrl = localUrl(QStringLiteral("/resolved/first/current.mp4"));
    const QUrl secondCurrentUrl = localUrl(QStringLiteral("/resolved/second/current.mp4"));
    const kiriview::ResolvedNavigationSource firstSource(requestedCurrentUrl, {}, firstCurrentUrl);
    const kiriview::ResolvedNavigationSource secondSource(
        requestedCurrentUrl, {}, secondCurrentUrl);
    const std::vector firstCandidates {
        directMediaNavigationCandidate(firstCurrentUrl),
        directMediaNavigationCandidate(firstAdjacentImageUrl),
    };
    const std::vector secondCandidates {
        directMediaNavigationCandidate(secondCurrentUrl),
        directMediaNavigationCandidate(secondAdjacentImageUrl),
    };
    const std::optional<kiriview::DirectMediaScope> firstScope
        = kiriview::DirectMediaScope::fromSource(firstSource, 4);
    const std::optional<kiriview::DirectMediaScope> secondScope
        = kiriview::DirectMediaScope::fromSource(secondSource, 4);
    QVERIFY(firstScope.has_value());
    QVERIFY(secondScope.has_value());
    const kiriview::DisplayedImageLocation firstAdjacentLocation
        = displayedLocation(firstAdjacentImageUrl, *firstScope);
    const kiriview::DisplayedImageLocation secondAdjacentLocation
        = displayedLocation(secondAdjacentImageUrl, *secondScope);

    coordinator.schedule(predecodeContext(
        requestedCurrentUrl, directMediaNavigationCandidateSnapshot(firstSource, firstCandidates)));
    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, firstAdjacentImageUrl);
    QVERIFY(dataLoader.finishOldestActiveLoadForUrl(
        firstAdjacentImageUrl, QByteArrayLiteral("first-scope")));
    QTRY_VERIFY(coordinator.findPredecodedImage(firstAdjacentLocation).has_value());
    QVERIFY(!coordinator.findPredecodedImage(secondAdjacentLocation).has_value());

    coordinator.schedule(predecodeContext(requestedCurrentUrl,
        directMediaNavigationCandidateSnapshot(secondSource, secondCandidates)));

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(2));
    QCOMPARE(dataLoader.backLoad().url, secondAdjacentImageUrl);
}

void TestMediaPredecodeCoordinator::powerSaverSuppressesLoadsButRetainsDisplayedImages()
{
    ManualImageDataLoader dataLoader;
    ManualPowerSaverMonitor* powerSaverMonitor = nullptr;
    ManualTimerScheduler timerScheduler;
    kiriview::MediaPredecodeCoordinator coordinator = createCoordinator(
        dataLoader, powerSaverProviderFor(powerSaverMonitor, true), timerScheduler.scheduler());
    QVERIFY(powerSaverMonitor != nullptr);

    const QUrl displayedUrl = localUrl(QStringLiteral("/media/00.png"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    timerScheduler.advanceTo(1000);
    coordinator.schedule(predecodeContext(displayedUrl,
        directMediaNavigationCandidateSnapshot(mixedDirectMediaNavigationCandidates()),
        { displayedImage(displayedUrl) }));

    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));

    timerScheduler.advanceTo(1200);
    powerSaverMonitor->setPowerSaverEnabled(false);

    QVERIFY(timerScheduler.timerAt(0).active());
    timerScheduler.timerAt(0).fire();

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
    QVERIFY(coordinator.findPredecodedImage(displayedLocation(displayedUrl)).has_value());
}

void TestMediaPredecodeCoordinator::powerSaverReschedulesVideoCursorWithoutDisplayedImages()
{
    ManualImageDataLoader dataLoader;
    ManualPowerSaverMonitor* powerSaverMonitor = nullptr;
    ManualTimerScheduler timerScheduler;
    kiriview::MediaPredecodeCoordinator coordinator = createCoordinator(
        dataLoader, powerSaverProviderFor(powerSaverMonitor, true), timerScheduler.scheduler());
    QVERIFY(powerSaverMonitor != nullptr);

    const QUrl videoUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextUrl = localUrl(QStringLiteral("/media/02.png"));
    timerScheduler.advanceTo(1000);
    const std::vector candidateRows = mixedDirectMediaNavigationCandidates();
    coordinator.schedule(predecodeContext(videoUrl,
        directMediaNavigationCandidateSnapshot(
            kiriview::ResolvedNavigationSource(videoUrl, {}, videoUrl), candidateRows)));

    QCOMPARE(dataLoader.loadCount(), std::size_t(0));

    timerScheduler.advanceTo(1200);
    powerSaverMonitor->setPowerSaverEnabled(false);

    QVERIFY(timerScheduler.timerAt(0).active());
    timerScheduler.timerAt(0).fire();

    QTRY_COMPARE(dataLoader.loadCount(), std::size_t(1));
    QCOMPARE(dataLoader.frontLoad().url, nextUrl);
}

void TestMediaPredecodeCoordinator::invalidScheduleClearsSuppressedDirectMediaNavigationCandidates()
{
    ManualImageDataLoader dataLoader;
    ManualPowerSaverMonitor* powerSaverMonitor = nullptr;
    ManualTimerScheduler timerScheduler;
    kiriview::MediaPredecodeCoordinator coordinator = createCoordinator(
        dataLoader, powerSaverProviderFor(powerSaverMonitor, true), timerScheduler.scheduler());
    QVERIFY(powerSaverMonitor != nullptr);

    timerScheduler.advanceTo(1000);
    const QUrl videoUrl = localUrl(QStringLiteral("/media/01.mp4"));
    const std::vector candidateRows = mixedDirectMediaNavigationCandidates();
    coordinator.schedule(predecodeContext(videoUrl,
        directMediaNavigationCandidateSnapshot(
            kiriview::ResolvedNavigationSource(videoUrl, {}, videoUrl), candidateRows)));
    coordinator.schedule(kiriview::MediaPredecodeCoordinator::Context {});

    powerSaverMonitor->setPowerSaverEnabled(false);

    QVERIFY(!timerScheduler.timerAt(0).active());
    QCOMPARE(dataLoader.loadCount(), std::size_t(0));
}

QTEST_GUILESS_MAIN(TestMediaPredecodeCoordinator)

#include "tst_mediapredecodecoordinator.moc"
