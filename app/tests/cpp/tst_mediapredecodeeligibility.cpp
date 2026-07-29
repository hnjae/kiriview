// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "image_test_support.h"
#include "predecode/mediapredecodeeligibility.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <optional>
#include <vector>

namespace {
using kiriview::TestSupport::localUrl;

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

kiriview::DirectMediaPageScopeIdentity directMediaPageScopeIdentity(const QUrl& url)
{
    const std::optional<kiriview::DirectMediaPageScopeIdentity> identity
        = kiriview::directMediaPageScopeIdentityForSource(
            kiriview::ResolvedNavigationSource(url, {}, url));
    Q_ASSERT(identity.has_value());
    return *identity;
}
}

class TestMediaPredecodeEligibility : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void snapshotKeepsMixedMediaIndicesForStillImages();
    void snapshotExcludesCandidatesOutsideOwnerParent();
    void targetIndicesReturnOnlyEligibleStillImages();
};

void TestMediaPredecodeEligibility::snapshotKeepsMixedMediaIndicesForStillImages()
{
    const QUrl previousImage = localUrl(QStringLiteral("/media/00.png"));
    const QUrl currentVideo = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextImage = localUrl(QStringLiteral("/media/02.png"));
    const QUrl laterVideo = localUrl(QStringLiteral("/media/03.mov"));
    const QUrl laterImage = localUrl(QStringLiteral("/media/04.jpg"));

    const kiriview::MediaPredecodeEligibilitySnapshot snapshot
        = kiriview::mediaPredecodeEligibilitySnapshot(
            {
                directMediaNavigationCandidate(previousImage),
                directMediaNavigationCandidate(currentVideo),
                directMediaNavigationCandidate(nextImage),
                directMediaNavigationCandidate(laterVideo),
                directMediaNavigationCandidate(laterImage),
            },
            directMediaPageScopeIdentity(currentVideo));

    QCOMPARE(snapshot.directMediaNavigationCandidateCount, std::size_t(5));
    QVERIFY(snapshot.currentMediaIndex.has_value());
    QCOMPARE(*snapshot.currentMediaIndex, std::size_t(1));
    QCOMPARE(snapshot.images.size(), std::size_t(3));
    QCOMPARE(snapshot.images.at(0).location.imageUrl(), previousImage);
    QCOMPARE(snapshot.images.at(0).mediaIndex, std::size_t(0));
    QCOMPARE(snapshot.images.at(1).location.imageUrl(), nextImage);
    QCOMPARE(snapshot.images.at(1).mediaIndex, std::size_t(2));
    QCOMPARE(snapshot.images.at(2).location.imageUrl(), laterImage);
    QCOMPARE(snapshot.images.at(2).mediaIndex, std::size_t(4));
}

void TestMediaPredecodeEligibility::snapshotExcludesCandidatesOutsideOwnerParent()
{
    const QUrl currentImage = localUrl(QStringLiteral("/media/current.png"));
    const QUrl adjacentImage = localUrl(QStringLiteral("/media/adjacent.png"));
    const QUrl foreignImage = localUrl(QStringLiteral("/other/foreign.png"));

    const kiriview::MediaPredecodeEligibilitySnapshot snapshot
        = kiriview::mediaPredecodeEligibilitySnapshot(
            {
                directMediaNavigationCandidate(currentImage),
                directMediaNavigationCandidate(foreignImage),
                directMediaNavigationCandidate(adjacentImage),
            },
            directMediaPageScopeIdentity(currentImage));

    QCOMPARE(snapshot.directMediaNavigationCandidateCount, std::size_t(3));
    QVERIFY(snapshot.currentMediaIndex.has_value());
    QCOMPARE(*snapshot.currentMediaIndex, std::size_t(0));
    QCOMPARE(snapshot.images.size(), std::size_t(2));
    QCOMPARE(snapshot.images.at(0).location.imageUrl(), currentImage);
    QCOMPARE(snapshot.images.at(0).mediaIndex, std::size_t(0));
    QCOMPARE(snapshot.images.at(1).location.imageUrl(), adjacentImage);
    QCOMPARE(snapshot.images.at(1).mediaIndex, std::size_t(2));
}

void TestMediaPredecodeEligibility::targetIndicesReturnOnlyEligibleStillImages()
{
    const QUrl previousImage = localUrl(QStringLiteral("/media/00.png"));
    const QUrl currentVideo = localUrl(QStringLiteral("/media/01.mp4"));
    const QUrl nextImage = localUrl(QStringLiteral("/media/02.png"));
    const QUrl laterImage = localUrl(QStringLiteral("/media/04.jpg"));
    const kiriview::MediaPredecodeEligibilitySnapshot snapshot
        = kiriview::mediaPredecodeEligibilitySnapshot(
            {
                directMediaNavigationCandidate(previousImage),
                directMediaNavigationCandidate(currentVideo),
                directMediaNavigationCandidate(nextImage),
                directMediaNavigationCandidate(localUrl(QStringLiteral("/media/03.mov"))),
                directMediaNavigationCandidate(laterImage),
            },
            directMediaPageScopeIdentity(currentVideo));

    const std::vector<kiriview::DisplayedImageLocation> locations
        = kiriview::mediaPredecodeEligibleLocationsForTargetIndices(snapshot, { 2, 1, 4, 99, 0 });

    QCOMPARE(locations.size(), std::size_t(3));
    QCOMPARE(locations.at(0).imageUrl(), nextImage);
    QCOMPARE(locations.at(1).imageUrl(), laterImage);
    QCOMPARE(locations.at(2).imageUrl(), previousImage);
}

QTEST_GUILESS_MAIN(TestMediaPredecodeEligibility)

#include "tst_mediapredecodeeligibility.moc"
