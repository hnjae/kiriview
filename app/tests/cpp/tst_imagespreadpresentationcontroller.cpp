// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "presentation/imagespreadpresentationcontroller.h"

#include "image_test_support.h"

#include <QObject>
#include <QTest>
#include <map>
#include <optional>
#include <vector>

namespace {
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::staticDisplayTestImagePayload;
using kiriview::TestSupport::testImage;

kiriview::OpenedCollectionScopeLocation openedCollectionScope()
{
    return kiriview::OpenedCollectionScopeLocation::fromUrls(
        localUrl(QStringLiteral("/books/book.cbz")), localUrl(QStringLiteral("/books/")),
        kiriview::OpenedCollectionScopeKind::ComicBookArchive);
}

kiriview::DisplayedImageLocation displayedLocation(const QUrl& url)
{
    return kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
        url, openedCollectionScope());
}

kiriview::ImageDocumentPageNavigationSnapshot navigationSnapshot(
    const std::vector<QUrl>& urls, int currentPageNumber)
{
    std::vector<kiriview::ImageDocumentPageTarget> targets;
    targets.reserve(urls.size());
    for (const QUrl& url : urls) {
        targets.emplace_back(url);
    }
    return { kiriview::PageNavigationState { std::move(targets), currentPageNumber - 1 } };
}

class SpreadFixture
{
public:
    SpreadFixture()
        : controller(state,
              kiriview::ImageSpreadPresentationController::Callbacks {
                  {},
                  [this](const kiriview::DisplayedImageLocation& location) {
                      return findPredecodedImage(location);
                  },
                  [this]() { return snapshot; },
                  [this]() { ++predecodeScheduleCount; },
                  [this](kiriview::ImageLoadSession session,
                      std::optional<kiriview::PredecodedImage> predecoded) {
                      ++preparedCount;
                      QVERIFY(predecoded.has_value());
                      controller.finishViewportSecondaryPageLoad(
                          session, predecoded->displayImage.originalSize);
                  },
                  [this]() {
                      ++clearCount;
                      if (replacementSession.has_value()) {
                          replacementPairingResult = controller.beginPageReplacementPairing(
                              *replacementSession, replacementPrimarySize);
                      }
                  },
                  {},
              })
    {
        state.setDisplayedImageLocation(displayedLocation(pageUrls.front()));
    }

    void displayPrimary(int pageNumber, QSize size)
    {
        snapshot = navigationSnapshot(pageUrls, pageNumber);
        state.setDisplayedImageLocation(displayedLocation(pageUrls.at(pageNumber - 1)));
        controller.commitPrimaryPageSlot(state.displayedImageLocation(), size);
    }

    std::optional<kiriview::PredecodedImage> findPredecodedImage(
        const kiriview::DisplayedImageLocation& location) const
    {
        const auto found = predecodedSizes.find(location.imageUrl());
        if (found == predecodedSizes.end()) {
            return std::nullopt;
        }
        return kiriview::PredecodedImage {
            staticDisplayTestImagePayload(testImage(found->second)),
            location,
        };
    }

    QObject context;
    kiriview::ImageDocumentState state;
    std::vector<QUrl> pageUrls {
        localUrl(QStringLiteral("/books/001.png")),
        localUrl(QStringLiteral("/books/002.png")),
        localUrl(QStringLiteral("/books/003.png")),
        localUrl(QStringLiteral("/books/004.png")),
        localUrl(QStringLiteral("/books/005.png")),
        localUrl(QStringLiteral("/books/006.png")),
    };
    std::map<QUrl, QSize> predecodedSizes;
    kiriview::ImageDocumentPageNavigationSnapshot snapshot;
    int preparedCount = 0;
    int clearCount = 0;
    int predecodeScheduleCount = 0;
    std::optional<kiriview::ImageLoadSession> replacementSession;
    QSize replacementPrimarySize;
    kiriview::ImageSpreadPageReplacementPairingResult replacementPairingResult
        = kiriview::ImageSpreadPageReplacementPairingResult::Stale;
    kiriview::ImageSpreadPresentationController controller;
};
}

class TestImageSpreadPresentationController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pagePairingAndWidthCacheRemainApplicationOwned();
    void shapeChangeSubmitsRequestedTargets();
    void pendingReplacementReplansWhenTwoPageModeIsReenabled();
};

void TestImageSpreadPresentationController::pagePairingAndWidthCacheRemainApplicationOwned()
{
    SpreadFixture fixture;
    fixture.displayPrimary(4, QSize(1200, 800));
    fixture.controller.setTwoPageModeEnabled(true);
    QVERIFY(fixture.controller.twoPageModeActive());
    QVERIFY(!fixture.controller.secondaryPageVisible());

    fixture.predecodedSizes[fixture.pageUrls.at(5)] = QSize(800, 1200);
    fixture.displayPrimary(5, QSize(800, 1200));
    fixture.controller.refreshSecondaryPage();
    QVERIFY(fixture.controller.secondaryPageVisible());

    const kiriview::ImageSpreadPageNavigationTarget target
        = fixture.controller.imageDocumentPageNavigationTarget(
            kiriview::NavigationDirection::Previous);
    QVERIFY(target.handledBySpread);
    QCOMPARE(target.pageNumber, 4);
}

void TestImageSpreadPresentationController::shapeChangeSubmitsRequestedTargets()
{
    SpreadFixture fixture;
    fixture.displayPrimary(5, QSize(800, 1200));
    fixture.predecodedSizes[fixture.pageUrls.at(5)] = QSize(800, 1200);
    const quint64 initialRevision = fixture.state.presentationLifecycleRevision();

    fixture.controller.setTwoPageModeEnabled(true);

    QCOMPARE(fixture.preparedCount, 1);
    QVERIFY(fixture.controller.secondaryPageVisible());
    QVERIFY(fixture.state.presentationLifecycleRevision() != initialRevision);
    const quint64 twoPageRevision = fixture.state.presentationLifecycleRevision();

    fixture.controller.setTwoPageModeEnabled(false);

    QCOMPARE(fixture.clearCount, 1);
    QVERIFY(!fixture.controller.secondaryPageVisible());
    QVERIFY(fixture.state.presentationLifecycleRevision() != twoPageRevision);
}

void TestImageSpreadPresentationController::pendingReplacementReplansWhenTwoPageModeIsReenabled()
{
    constexpr quint64 primarySessionId = 41;
    const QSize portraitSize(800, 1200);
    SpreadFixture fixture;
    fixture.predecodedSizes[fixture.pageUrls.at(2)] = portraitSize;
    fixture.predecodedSizes[fixture.pageUrls.at(4)] = portraitSize;
    fixture.displayPrimary(2, portraitSize);
    fixture.controller.setTwoPageModeEnabled(true);
    QVERIFY(fixture.controller.secondaryPageVisible());

    fixture.snapshot = navigationSnapshot(fixture.pageUrls, 4);
    const QUrl replacementUrl = fixture.pageUrls.at(3);
    const kiriview::ImageLoadRequest request = kiriview::ImageLoadRequest::fromSameScopePageTarget(
        kiriview::ImageDocumentPageTarget {
            replacementUrl, kiriview::ImageDocumentPageKind::Image },
        openedCollectionScope());
    fixture.replacementSession
        = kiriview::ImageLoadSession(primarySessionId, request, displayedLocation(replacementUrl));
    fixture.replacementPrimarySize = portraitSize;

    QCOMPARE(fixture.controller.beginPageReplacementPairing(
                 *fixture.replacementSession, fixture.replacementPrimarySize),
        kiriview::ImageSpreadPageReplacementPairingResult::PreparingSecondary);
    QVERIFY(fixture.controller.pageReplacementPairingPending());

    fixture.controller.setTwoPageModeEnabled(false);
    QCOMPARE(fixture.clearCount, 1);
    QCOMPARE(fixture.replacementPairingResult,
        kiriview::ImageSpreadPageReplacementPairingResult::PrimaryOnly);
    QVERIFY(fixture.controller.pageReplacementPairingPending());

    fixture.controller.setTwoPageModeEnabled(true);
    QCOMPARE(fixture.clearCount, 2);
    QCOMPARE(fixture.replacementPairingResult,
        kiriview::ImageSpreadPageReplacementPairingResult::PreparingSecondary);
    QVERIFY(fixture.controller.pageReplacementPairingPending());
}

QTEST_GUILESS_MAIN(TestImageSpreadPresentationController)

#include "tst_imagespreadpresentationcontroller.moc"
