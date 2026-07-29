// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "candidate_test_support.h"
#include "location/imagedocumentlocation.h"
#include "predecode/predecodewindowplan.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <optional>
#include <vector>

namespace {
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::imagesDirectoryUrl;
using kiriview::TestSupport::indexedImageUrl;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::videoCandidate;

std::vector<kiriview::ImageDocumentPageCandidate> imageDocumentPageCandidates(int count)
{
    std::vector<kiriview::ImageDocumentPageCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        candidates.push_back(imageDocumentPageCandidate(indexedImageUrl(index)));
    }
    return candidates;
}

kiriview::PredecodePolicyInput policyInputForLocation(
    const kiriview::DisplayedImageLocation& location,
    kiriview::PredecodeMomentumMode momentumMode = kiriview::PredecodeMomentumMode::Neutral,
    bool powerSaverEnabled = false, int idealThreadCount = 4)
{
    return kiriview::PredecodePolicyInput {
        kiriview::predecodeSourceProfileForOpenedCollectionScope(
            location.openedCollectionScope(), idealThreadCount),
        momentumMode,
        powerSaverEnabled,
    };
}

kiriview::PredecodeWindowStartPlan startPlanForLocation(
    const kiriview::DisplayedImageLocation& location,
    kiriview::PredecodeMomentumMode momentumMode = kiriview::PredecodeMomentumMode::Neutral,
    bool powerSaverEnabled = false, int idealThreadCount = 4)
{
    return kiriview::predecodeWindowStartPlan(kiriview::PredecodeWindowPlanRequest {
        location,
        policyInputForLocation(location, momentumMode, powerSaverEnabled, idealThreadCount),
    });
}

}

class TestPredecodeWindowPlan : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void regularImagePlansCandidateContextAndNeutralWindow();
    void powerSaverSuppressesCandidateLoading();
    void missingCandidateContextStillCarriesFallbackWindow();
    void directoryCollectionUsesDocumentParallelLimit();
    void predecodeWindowSkipsOpenedCollectionVideoCandidates();
    void archiveWindowPreservesOpenedCollectionScopeContext();
    void missingCurrentCandidateYieldsEmptyWindow();
    void candidateListingFailureUsesPlannedFallbackWindow();
};

void TestPredecodeWindowPlan::regularImagePlansCandidateContextAndNeutralWindow()
{
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(5));
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    QVERIFY(startPlan.shouldLoadCandidates());
    QCOMPARE(startPlan.fallbackWindow.parallelLimit, std::size_t(1));
    QVERIFY(startPlan.fallbackWindow.locations.empty());
    QVERIFY(startPlan.candidateList.has_value());
    QCOMPARE(startPlan.candidateList->context.currentUrl(), indexedImageUrl(5));

    const kiriview::PredecodeWindowPlan windowPlan
        = kiriview::predecodeWindowPlanForCandidates(startPlan, imageDocumentPageCandidates(15));

    QCOMPARE(windowPlan.parallelLimit, std::size_t(1));
    QCOMPARE(windowPlan.locations.size(), std::size_t(5));
    QCOMPARE(windowPlan.locations.at(0).imageUrl(), indexedImageUrl(5));
    QCOMPARE(windowPlan.locations.at(1).imageUrl(), indexedImageUrl(6));
    QCOMPARE(windowPlan.locations.at(2).imageUrl(), indexedImageUrl(4));
    QCOMPARE(windowPlan.locations.at(3).imageUrl(), indexedImageUrl(7));
    QCOMPARE(windowPlan.locations.at(4).imageUrl(), indexedImageUrl(3));
    QVERIFY(location.directMediaPageScopeIdentity().has_value());
    for (const kiriview::DisplayedImageLocation& candidateLocation : windowPlan.locations) {
        QVERIFY(candidateLocation.directMediaPageScopeIdentity().has_value());
        QVERIFY(
            kiriview::sameSourceKey(candidateLocation.directMediaPageScopeIdentity()->parentKey(),
                location.directMediaPageScopeIdentity()->parentKey()));
    }
}

void TestPredecodeWindowPlan::powerSaverSuppressesCandidateLoading()
{
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(5));
    const kiriview::PredecodeWindowStartPlan startPlan
        = startPlanForLocation(location, kiriview::PredecodeMomentumMode::Neutral, true);

    QVERIFY(!startPlan.shouldLoadCandidates());
    QVERIFY(!startPlan.candidateList.has_value());

    const kiriview::PredecodeWindowPlan windowPlan = startPlan.fallbackWindow;
    QCOMPARE(windowPlan.parallelLimit, std::size_t(0));
    QVERIFY(windowPlan.locations.empty());
}

void TestPredecodeWindowPlan::missingCandidateContextStillCarriesFallbackWindow()
{
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(QUrl());
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    QVERIFY(!startPlan.shouldLoadCandidates());
    QVERIFY(!startPlan.candidateList.has_value());

    const kiriview::PredecodeWindowPlan windowPlan = startPlan.fallbackWindow;
    QCOMPARE(windowPlan.parallelLimit, std::size_t(1));
    QVERIFY(windowPlan.locations.empty());
}

void TestPredecodeWindowPlan::directoryCollectionUsesDocumentParallelLimit()
{
    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(imagesDirectoryUrl(),
            imagesDirectoryUrl(), kiriview::OpenedCollectionScopeKind::Directory);
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            indexedImageUrl(5), directoryCollection);
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    QVERIFY(startPlan.shouldLoadCandidates());

    const kiriview::PredecodeWindowPlan windowPlan
        = kiriview::predecodeWindowPlanForCandidates(startPlan, imageDocumentPageCandidates(15));

    QCOMPARE(windowPlan.parallelLimit, std::size_t(2));
    QCOMPARE(windowPlan.locations.size(), std::size_t(8));
    QCOMPARE(windowPlan.locations.at(0).imageUrl(), indexedImageUrl(5));
    QCOMPARE(windowPlan.locations.at(1).imageUrl(), indexedImageUrl(6));
    QCOMPARE(windowPlan.locations.at(2).imageUrl(), indexedImageUrl(4));
    QCOMPARE(windowPlan.locations.at(3).imageUrl(), indexedImageUrl(7));
    QCOMPARE(windowPlan.locations.at(4).imageUrl(), indexedImageUrl(3));
    QCOMPARE(windowPlan.locations.at(5).imageUrl(), indexedImageUrl(8));
    QCOMPARE(windowPlan.locations.at(6).imageUrl(), indexedImageUrl(2));
    QCOMPARE(windowPlan.locations.at(7).imageUrl(), indexedImageUrl(9));
    for (const kiriview::DisplayedImageLocation& candidateLocation : windowPlan.locations) {
        QCOMPARE(candidateLocation.openedCollectionScope(), directoryCollection);
    }
}

void TestPredecodeWindowPlan::predecodeWindowSkipsOpenedCollectionVideoCandidates()
{
    const kiriview::OpenedCollectionScopeLocation directoryCollection
        = kiriview::OpenedCollectionScopeLocation::fromUrls(imagesDirectoryUrl(),
            imagesDirectoryUrl(), kiriview::OpenedCollectionScopeKind::Directory);
    const QUrl displayedUrl = indexedImageUrl(1);
    const QUrl videoUrl = localUrl(QStringLiteral("/images/02.mp4"));
    const QUrl nextImageUrl = indexedImageUrl(3);
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            displayedUrl, directoryCollection);
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    const kiriview::PredecodeWindowPlan windowPlan
        = kiriview::predecodeWindowPlanForCandidates(startPlan,
            {
                imageDocumentPageCandidate(displayedUrl),
                videoCandidate(videoUrl),
                imageDocumentPageCandidate(nextImageUrl),
            });

    QCOMPARE(windowPlan.locations.size(), std::size_t(2));
    QCOMPARE(windowPlan.locations.at(0).imageUrl(), displayedUrl);
    QCOMPARE(windowPlan.locations.at(1).imageUrl(), nextImageUrl);
}

void TestPredecodeWindowPlan::archiveWindowPreservesOpenedCollectionScopeContext()
{
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> openedCollectionScope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(openedCollectionScope.has_value());

    const QUrl displayedUrl
        = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("01.png"));
    const QUrl nextUrl = archivePageUrl(openedCollectionScope->rootUrl(), QStringLiteral("02.png"));
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromOpenedCollectionScope(
            displayedUrl, *openedCollectionScope);
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    QVERIFY(startPlan.shouldLoadCandidates());

    const kiriview::PredecodeWindowPlan windowPlan
        = kiriview::predecodeWindowPlanForCandidates(startPlan,
            {
                imageDocumentPageCandidate(displayedUrl),
                imageDocumentPageCandidate(nextUrl),
            });

    QCOMPARE(windowPlan.parallelLimit, std::size_t(2));
    QCOMPARE(windowPlan.locations.size(), std::size_t(2));
    QCOMPARE(windowPlan.locations.at(0).imageUrl(), displayedUrl);
    QCOMPARE(windowPlan.locations.at(1).imageUrl(), nextUrl);
    QCOMPARE(windowPlan.locations.at(0).openedCollectionScope(), *openedCollectionScope);
    QCOMPARE(windowPlan.locations.at(1).openedCollectionScope(), *openedCollectionScope);
}

void TestPredecodeWindowPlan::missingCurrentCandidateYieldsEmptyWindow()
{
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(5));
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    const kiriview::PredecodeWindowPlan windowPlan
        = kiriview::predecodeWindowPlanForCandidates(startPlan,
            {
                imageDocumentPageCandidate(indexedImageUrl(0)),
                imageDocumentPageCandidate(indexedImageUrl(1)),
                imageDocumentPageCandidate(indexedImageUrl(2)),
            });

    QCOMPARE(windowPlan.parallelLimit, std::size_t(1));
    QVERIFY(windowPlan.locations.empty());
}

void TestPredecodeWindowPlan::candidateListingFailureUsesPlannedFallbackWindow()
{
    const kiriview::DisplayedImageLocation location
        = kiriview::DisplayedImageLocation::fromUrl(indexedImageUrl(5));
    const kiriview::PredecodeWindowStartPlan startPlan = startPlanForLocation(location);

    QVERIFY(startPlan.shouldLoadCandidates());
    QCOMPARE(startPlan.fallbackWindow.parallelLimit, std::size_t(1));
    QVERIFY(startPlan.fallbackWindow.locations.empty());

    const kiriview::PredecodeWindowPlan fallbackWindow = kiriview::predecodeWindowPlanForCandidates(
        kiriview::PredecodeWindowStartPlan {
            startPlan.fallbackWindow,
            std::nullopt,
        },
        imageDocumentPageCandidates(15));

    QCOMPARE(fallbackWindow.parallelLimit, startPlan.fallbackWindow.parallelLimit);
    QVERIFY(fallbackWindow.locations.empty());
}

QTEST_GUILESS_MAIN(TestPredecodeWindowPlan)

#include "tst_predecodewindowplan.moc"
