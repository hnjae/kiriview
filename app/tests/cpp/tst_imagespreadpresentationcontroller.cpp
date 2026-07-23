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
                  [this](const QUrl& url) { return findPredecodedImage(url); },
                  [this]() { return snapshot; },
                  [this]() { ++predecodeScheduleCount; },
                  [this](kiriview::ImageLoadSession session,
                      std::optional<kiriview::PredecodedImage> predecoded) {
                      ++preparedCount;
                      QVERIFY(predecoded.has_value());
                      controller.finishViewportSecondaryPageLoad(
                          session, predecoded->displayImage.originalSize);
                  },
                  [this]() { ++clearCount; },
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

    std::optional<kiriview::PredecodedImage> findPredecodedImage(const QUrl& url) const
    {
        const auto found = predecodedSizes.find(url);
        if (found == predecodedSizes.end()) {
            return std::nullopt;
        }
        return kiriview::PredecodedImage {
            staticDisplayTestImagePayload(testImage(found->second)),
            displayedLocation(url),
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
    kiriview::ImageSpreadPresentationController controller;
};
}

class TestImageSpreadPresentationController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pagePairingAndWidthCacheRemainApplicationOwned();
    void shapeChangeSubmitsRequestedTargets();
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

    fixture.controller.setTwoPageModeEnabled(true);

    QCOMPARE(fixture.preparedCount, 1);
    QVERIFY(fixture.controller.secondaryPageVisible());

    fixture.controller.setTwoPageModeEnabled(false);

    QCOMPARE(fixture.clearCount, 1);
    QVERIFY(!fixture.controller.secondaryPageVisible());
}

QTEST_GUILESS_MAIN(TestImageSpreadPresentationController)

#include "tst_imagespreadpresentationcontroller.moc"
