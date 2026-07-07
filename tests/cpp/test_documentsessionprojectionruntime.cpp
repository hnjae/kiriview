// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "session/documentsessionprojectionruntime.h"

#include <QObject>
#include <QTest>
#include <QUrl>

#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::DirectMediaNavigationCandidate directMediaNavigationCandidate(const QUrl& url)
{
    return kiriview::DirectMediaNavigationCandidate { url, url.fileName(QUrl::PrettyDecoded) };
}

kiriview::DirectMediaNavigationCandidateSnapshot directMediaNavigationCandidateSnapshot(
    std::vector<kiriview::DirectMediaNavigationCandidate> candidates, quint64 revision = 1)
{
    kiriview::DirectMediaNavigationCandidateSnapshot snapshot;
    snapshot.source.currentUrl = candidates.empty() ? QUrl() : candidates.front().url;
    snapshot.source.parentUrl = QUrl::fromLocalFile(QStringLiteral("/media"));
    snapshot.source.generation = 9;
    snapshot.revision = revision;
    snapshot.candidates
        = std::make_shared<const std::vector<kiriview::DirectMediaNavigationCandidate>>(
            std::move(candidates));
    snapshot.boundaryState.currentNumber = 2;
    snapshot.boundaryState.count = static_cast<int>(snapshot.candidates->size());
    snapshot.known = true;
    return snapshot;
}

static_assert(std::is_same_v<decltype(kiriview::DocumentSessionProjectionRuntimePorts::
                                     directMediaNavigationCandidateSnapshot),
    std::function<const kiriview::DirectMediaNavigationCandidateSnapshot&()>>);
}

class TestDocumentSessionProjectionRuntime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void publishCommitsSnapshotBeforeThumbnailRowsAndRevealCleanup();
    void sourceKindPublishSkipsThumbnailRowsWhenRejected();
};

void TestDocumentSessionProjectionRuntime::
    publishCommitsSnapshotBeforeThumbnailRowsAndRevealCleanup()
{
    std::vector<QString> events;
    kiriview::ActiveNavigationSourceKind committedSourceKind
        = kiriview::ActiveNavigationSourceKind::None;
    kiriview::ActiveNavigationSnapshot committedNavigation;
    kiriview::DirectMediaNavigationCandidateSnapshot committedCandidateSnapshot;
    std::vector<kiriview::ActiveNavigationThumbnailRow> publishedRows;

    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot
        = [&events, &committedSourceKind, &committedNavigation, &committedCandidateSnapshot](
              const kiriview::DocumentSessionPublicSnapshotInput& input) {
              events.push_back(QStringLiteral("commit:%1").arg(input.inputRevision));
              committedSourceKind = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
              committedNavigation.available = true;
              committedNavigation.known = true;
              committedNavigation.currentNumber = 2;
              committedNavigation.count = 2;
              committedCandidateSnapshot = directMediaNavigationCandidateSnapshot(
                  {
                      directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.png"))),
                      directMediaNavigationCandidate(localUrl(QStringLiteral("/media/02.mp4"))),
                  },
                  42);
              return true;
          };
    ports.activeNavigationSourceKind = [&committedSourceKind]() { return committedSourceKind; };
    ports.activeNavigationSnapshot = [&committedNavigation]() { return committedNavigation; };
    ports.directMediaNavigationCandidateSnapshot
        = [&committedCandidateSnapshot]() -> const auto& { return committedCandidateSnapshot; };
    ports.setActiveNavigationThumbnailRows
        = [&events, &publishedRows](std::vector<kiriview::ActiveNavigationThumbnailRow> rows) {
              events.push_back(QStringLiteral("rows:%1").arg(rows.size()));
              publishedRows = std::move(rows);
          };
    ports.clearActiveNavigationRevealContextIfUnavailable
        = [&events]() { events.push_back(QStringLiteral("clear-reveal")); };

    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput input;
    input.inputRevision = 7;

    runtime.publish(input, {});

    const std::vector<QString> expected {
        QStringLiteral("commit:7"),
        QStringLiteral("rows:2"),
        QStringLiteral("clear-reveal"),
    };
    QCOMPARE(events, expected);
    QCOMPARE(publishedRows.size(), std::size_t(2));
    QCOMPARE(
        publishedRows.at(0).sourceKind, kiriview::ActiveNavigationThumbnailSourceKind::DirectImage);
    QCOMPARE(
        publishedRows.at(1).sourceKind, kiriview::ActiveNavigationThumbnailSourceKind::DirectVideo);
    QVERIFY(publishedRows.at(1).current);
}

void TestDocumentSessionProjectionRuntime::sourceKindPublishSkipsThumbnailRowsWhenRejected()
{
    std::vector<QString> events;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshotForSourceKind
        = [&events](const kiriview::DocumentSessionPublicSnapshotInput&,
              kiriview::ActiveNavigationSourceKind sourceKind) {
              events.push_back(QStringLiteral("commit-source-kind:%1")
                      .arg(sourceKind == kiriview::ActiveNavigationSourceKind::ImageDocumentPages));
              return false;
          };
    ports.setActiveNavigationThumbnailRows
        = [&events](std::vector<kiriview::ActiveNavigationThumbnailRow>) {
              events.push_back(QStringLiteral("rows"));
          };
    ports.clearActiveNavigationRevealContextIfUnavailable
        = [&events]() { events.push_back(QStringLiteral("clear-reveal")); };

    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));

    runtime.publishForSourceKind({}, kiriview::ActiveNavigationSourceKind::ImageDocumentPages, {});

    const std::vector<QString> expected {
        QStringLiteral("commit-source-kind:1"),
        QStringLiteral("clear-reveal"),
    };
    QCOMPARE(events, expected);
}

QTEST_GUILESS_MAIN(TestDocumentSessionProjectionRuntime)

#include "test_documentsessionprojectionruntime.moc"
