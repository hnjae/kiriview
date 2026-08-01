// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/activenavigationthumbnailprojection.h"

#include <QObject>
#include <QTest>
#include <QUrl>
#include <cstddef>
#include <memory>
#include <vector>

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(
    const QUrl& url, const QString& name = {})
{
    return kiriview::DirectMediaNavigationCandidate { url, name };
}

kiriview::DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot(
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates)
{
    kiriview::DirectMediaNavigationCandidateSnapshot snapshot;
    if (!candidates.empty()) {
        snapshot.source = kiriview::DirectMediaScope::fromSource(
            kiriview::resolvedNavigationSource(candidates.front().url, {}), 1);
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

kiriview::ImageDocumentPageCandidateListSnapshot imageDocumentPageCandidateListSnapshot(
    std::vector<kiriview::ImageDocumentPageCandidate> candidates)
{
    kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    snapshot.source = kiriview::ImageDocumentPageCandidateListSource::forDirectory(
        localUrl(QStringLiteral("/archive")));
    snapshot.revision = 1;
    snapshot.candidates = std::make_shared<const std::vector<kiriview::ImageDocumentPageCandidate>>(
        std::move(candidates));
    snapshot.known = true;
    return snapshot;
}

kiriview::ActiveNavigationSnapshot knownNavigation(int currentNumber, int count)
{
    return kiriview::ActiveNavigationSnapshot {
        true,
        true,
        true,
        currentNumber > 1,
        currentNumber < count,
        currentNumber == 1,
        currentNumber == count,
        currentNumber,
        count,
    };
}
}

class TestActiveNavigationThumbnailProjection : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void directMediaRowsUseConfirmedCandidates();
    void directMediaRowsDecodeFallbackLabels();
    void imageDocumentRowsUsePageCandidateListSnapshot();
    void unavailableUnknownAndMismatchedNavigationProjectNoRows();
};

void TestActiveNavigationThumbnailProjection::directMediaRowsUseConfirmedCandidates()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const QUrl videoUrl = localUrl(QStringLiteral("/media/02.mp4"));

    const std::vector<kiriview::ActiveNavigationThumbnailRow> rows
        = kiriview::projectActiveNavigationThumbnailRows(
            kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, knownNavigation(2, 2),
            directMediaNavigationCandidateSnapshot({
                directMediaNavigationCandidate(imageUrl),
                directMediaNavigationCandidate(videoUrl, QStringLiteral("Clip")),
            }),
            {});

    QCOMPARE(rows.size(), std::size_t(2));
    QCOMPARE(rows.at(0).number, 1);
    QCOMPARE(rows.at(0).url, imageUrl);
    QCOMPARE(rows.at(0).label, QStringLiteral("01.png"));
    QVERIFY(rows.at(0).kind == kiriview::ActiveNavigationThumbnailKind::Image);
    QVERIFY(!rows.at(0).current);

    QCOMPARE(rows.at(1).number, 2);
    QCOMPARE(rows.at(1).url, videoUrl);
    QCOMPARE(rows.at(1).label, QStringLiteral("Clip"));
    QVERIFY(rows.at(1).kind == kiriview::ActiveNavigationThumbnailKind::Video);
    QVERIFY(rows.at(1).current);
}

void TestActiveNavigationThumbnailProjection::directMediaRowsDecodeFallbackLabels()
{
    const QUrl videoUrl = localUrl(QStringLiteral("/media/[clip].mp4"));

    const std::vector<kiriview::ActiveNavigationThumbnailRow> rows
        = kiriview::projectActiveNavigationThumbnailRows(
            kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, knownNavigation(1, 1),
            directMediaNavigationCandidateSnapshot({ directMediaNavigationCandidate(videoUrl) }),
            {});

    QCOMPARE(rows.size(), std::size_t(1));
    QCOMPARE(rows.at(0).url, videoUrl);
    QCOMPARE(rows.at(0).label, QStringLiteral("[clip].mp4"));
}

void TestActiveNavigationThumbnailProjection::imageDocumentRowsUsePageCandidateListSnapshot()
{
    const QUrl firstPage = localUrl(QStringLiteral("/archive/01.png"));
    const QUrl secondPage = localUrl(QStringLiteral("/archive/clip.mp4"));
    const kiriview::ImageDocumentPageCandidateListSnapshot pageSnapshot
        = imageDocumentPageCandidateListSnapshot({
            kiriview::ImageDocumentPageCandidate {
                firstPage,
                QStringLiteral("chapter/01.png"),
                kiriview::ImageDocumentPageKind::Image,
            },
            kiriview::ImageDocumentPageCandidate {
                secondPage,
                {},
                kiriview::ImageDocumentPageKind::Video,
            },
        });

    const std::vector<kiriview::ActiveNavigationThumbnailRow> rows
        = kiriview::projectActiveNavigationThumbnailRows(
            kiriview::ActiveNavigationSourceKind::ImageDocumentPages, knownNavigation(1, 2),
            directMediaNavigationCandidateSnapshot({}), pageSnapshot);

    QCOMPARE(rows.size(), std::size_t(2));
    QCOMPARE(rows.at(0).number, 1);
    QCOMPARE(rows.at(0).url, firstPage);
    QCOMPARE(rows.at(0).label, QStringLiteral("chapter/01.png"));
    QVERIFY(rows.at(0).kind == kiriview::ActiveNavigationThumbnailKind::Image);
    QVERIFY(rows.at(0).current);

    QCOMPARE(rows.at(1).number, 2);
    QCOMPARE(rows.at(1).url, secondPage);
    QCOMPARE(rows.at(1).label, QStringLiteral("clip.mp4"));
    QVERIFY(rows.at(1).kind == kiriview::ActiveNavigationThumbnailKind::Video);
    QVERIFY(!rows.at(1).current);
}

void TestActiveNavigationThumbnailProjection::
    unavailableUnknownAndMismatchedNavigationProjectNoRows()
{
    const QUrl imageUrl = localUrl(QStringLiteral("/media/01.png"));
    const kiriview::DirectMediaNavigationCandidateSnapshot candidates
        = directMediaNavigationCandidateSnapshot({ directMediaNavigationCandidate(imageUrl) });

    QVERIFY(kiriview::projectActiveNavigationThumbnailRows(
        kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, {}, candidates, {})
            .empty());

    kiriview::ActiveNavigationSnapshot unknown;
    unknown.available = true;
    QVERIFY(kiriview::projectActiveNavigationThumbnailRows(
        kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, unknown, candidates, {})
            .empty());

    QVERIFY(kiriview::projectActiveNavigationThumbnailRows(
        kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia, knownNavigation(1, 2),
        candidates, {})
            .empty());
}

QTEST_GUILESS_MAIN(TestActiveNavigationThumbnailProjection)

#include "tst_activenavigationthumbnailprojection.moc"
