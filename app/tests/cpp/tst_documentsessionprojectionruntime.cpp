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
    if (!candidates.empty()) {
        snapshot.source = kiriview::DirectMediaScope::fromSource(
            kiriview::ResolvedNavigationSource(candidates.front().url, {}, candidates.front().url),
            9);
    }
    snapshot.revision = revision;
    snapshot.candidates
        = std::make_shared<const std::vector<kiriview::DirectMediaNavigationCandidate>>(
            std::move(candidates));
    snapshot.boundaryState.currentNumber = 2;
    snapshot.boundaryState.count = static_cast<int>(snapshot.candidates->size());
    snapshot.known = true;
    return snapshot;
}

kiriview::ActiveNavigationSnapshot activeNavigationSnapshot(int currentNumber, int count)
{
    kiriview::ActiveNavigationSnapshot snapshot;
    snapshot.available = true;
    snapshot.known = true;
    snapshot.editable = true;
    snapshot.currentNumber = currentNumber;
    snapshot.count = count;
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
    void sameDirectMediaCandidateRevisionUpdatesCurrentWithoutRows();
    void unchangedDependenciesSkipPublicAndThumbnailProjection();
    void publicOnlyChangesSkipThumbnailProjection();
    void actionStateInputsInvalidatePublicDependency();
    void candidateRevisionChangesThumbnailWithoutPublicProjection();
    void repeatedUnavailableThumbnailDependencyClearsRowsOnce();
    void mediaInformationRevisionDoesNotInvalidateSemanticDependency();
    void sourceKindPublishSkipsThumbnailRowsWhenRejected();
    void destructionDuringPublicSnapshotCommitStopsPublication();
    void nestedPublicationSupersedesOuterContinuation();
    void destructionDuringThumbnailRowsCommitStopsPublication();
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
              committedNavigation = activeNavigationSnapshot(2, 2);
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

void TestDocumentSessionProjectionRuntime::
    sameDirectMediaCandidateRevisionUpdatesCurrentWithoutRows()
{
    std::vector<QString> events;
    kiriview::ActiveNavigationSourceKind sourceKind
        = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
    kiriview::ActiveNavigationSnapshot navigation = activeNavigationSnapshot(1, 2);
    const kiriview::DirectMediaNavigationCandidateSnapshot candidateSnapshot
        = directMediaNavigationCandidateSnapshot({
            directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.png"))),
            directMediaNavigationCandidate(localUrl(QStringLiteral("/media/02.png"))),
        });

    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&events](const kiriview::DocumentSessionPublicSnapshotInput&) {
        events.push_back(QStringLiteral("commit"));
        return true;
    };
    ports.activeNavigationSourceKind = [&sourceKind]() { return sourceKind; };
    ports.activeNavigationSnapshot = [&navigation]() { return navigation; };
    ports.directMediaNavigationCandidateSnapshot
        = [&candidateSnapshot]() -> const auto& { return candidateSnapshot; };
    ports.setActiveNavigationThumbnailRows
        = [&events](std::vector<kiriview::ActiveNavigationThumbnailRow> rows) {
              events.push_back(QStringLiteral("rows:%1").arg(rows.size()));
          };
    ports.setActiveNavigationThumbnailCurrentNumber = [&events](int currentNumber) {
        events.push_back(QStringLiteral("current:%1").arg(currentNumber));
    };

    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));

    runtime.publish({}, {});
    navigation = activeNavigationSnapshot(2, 2);
    runtime.publish({}, {});

    const std::vector<QString> expected {
        QStringLiteral("commit"),
        QStringLiteral("rows:2"),
        QStringLiteral("current:2"),
    };
    QCOMPARE(events, expected);
}

void TestDocumentSessionProjectionRuntime::unchangedDependenciesSkipPublicAndThumbnailProjection()
{
    std::vector<QString> events;
    kiriview::ActiveNavigationSourceKind sourceKind
        = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
    const kiriview::ActiveNavigationSnapshot navigation = activeNavigationSnapshot(1, 1);
    const auto candidates = directMediaNavigationCandidateSnapshot(
        { directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.png"))) });
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&events](const auto&) {
        events.push_back(QStringLiteral("commit"));
        return true;
    };
    ports.activeNavigationSourceKind = [&sourceKind]() { return sourceKind; };
    ports.activeNavigationSnapshot = [&navigation]() { return navigation; };
    ports.directMediaNavigationCandidateSnapshot
        = [&candidates]() -> const auto& { return candidates; };
    ports.setActiveNavigationThumbnailRows
        = [&events](auto) { events.push_back(QStringLiteral("rows")); };
    ports.setActiveNavigationThumbnailCurrentNumber
        = [&events](int) { events.push_back(QStringLiteral("current")); };
    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput input;
    input.inputRevision = 1;
    input.session.documentKind = kiriview::DocumentSessionKind::Image;

    runtime.publish(input, {});
    input.inputRevision = 2;
    runtime.publish(input, {});

    QCOMPARE(events, (std::vector<QString> { QStringLiteral("commit"), QStringLiteral("rows") }));
}

void TestDocumentSessionProjectionRuntime::publicOnlyChangesSkipThumbnailProjection()
{
    std::vector<QString> events;
    kiriview::ActiveNavigationSourceKind sourceKind
        = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
    const kiriview::ActiveNavigationSnapshot navigation = activeNavigationSnapshot(1, 1);
    const auto candidates = directMediaNavigationCandidateSnapshot(
        { directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.png"))) });
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&events](const auto&) {
        events.push_back(QStringLiteral("commit"));
        return true;
    };
    ports.activeNavigationSourceKind = [&sourceKind]() { return sourceKind; };
    ports.activeNavigationSnapshot = [&navigation]() { return navigation; };
    ports.directMediaNavigationCandidateSnapshot
        = [&candidates]() -> const auto& { return candidates; };
    ports.setActiveNavigationThumbnailRows
        = [&events](auto) { events.push_back(QStringLiteral("rows")); };
    ports.setActiveNavigationThumbnailCurrentNumber
        = [&events](int) { events.push_back(QStringLiteral("current")); };
    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput input;
    input.session.documentKind = kiriview::DocumentSessionKind::Image;
    input.image.readyForInformation = true;
    input.image.zoomPercentKnown = true;
    input.image.zoomPercent = 100.0;

    runtime.publish(input, {});
    input.image.zoomPercent = 125.0;
    runtime.publish(input, {});

    QCOMPARE(events,
        (std::vector<QString> {
            QStringLiteral("commit"), QStringLiteral("rows"), QStringLiteral("commit") }));
}

void TestDocumentSessionProjectionRuntime::actionStateInputsInvalidatePublicDependency()
{
    std::vector<kiriview::DocumentSessionActionStateSnapshot> committedActionStates;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&committedActionStates](const auto& input) {
        committedActionStates.push_back(
            kiriview::projectDocumentSessionPublicSnapshot(input, 0).actionState);
        return true;
    };
    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput input;
    input.session.documentKind = kiriview::DocumentSessionKind::Image;

    runtime.publish(input, {});
    input.image.viewportPannable = true;
    runtime.publish(input, {});
    input.session.documentKind = kiriview::DocumentSessionKind::Video;
    runtime.publish(input, {});
    input.video.videoSeekable = true;
    runtime.publish(input, {});
    input.video.videoDuration = 42'000;
    runtime.publish(input, {});

    QCOMPARE(committedActionStates.size(), std::size_t(5));
    QVERIFY(!committedActionStates.at(0).imagePannable);
    QVERIFY(committedActionStates.at(1).imagePannable);
    QVERIFY(committedActionStates.at(2).videoMode);
    QVERIFY(!committedActionStates.at(2).videoSeekable);
    QCOMPARE(committedActionStates.at(2).videoDuration, qint64(0));
    QVERIFY(committedActionStates.at(3).videoSeekable);
    QCOMPARE(committedActionStates.at(4).videoDuration, qint64(42'000));
}

void TestDocumentSessionProjectionRuntime::
    candidateRevisionChangesThumbnailWithoutPublicProjection()
{
    std::vector<QString> events;
    kiriview::ActiveNavigationSourceKind sourceKind
        = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
    const kiriview::ActiveNavigationSnapshot navigation = activeNavigationSnapshot(1, 1);
    auto candidates = directMediaNavigationCandidateSnapshot(
        { directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.png"))) });
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&events](const auto&) {
        events.push_back(QStringLiteral("commit"));
        return true;
    };
    ports.activeNavigationSourceKind = [&sourceKind]() { return sourceKind; };
    ports.activeNavigationSnapshot = [&navigation]() { return navigation; };
    ports.directMediaNavigationCandidateSnapshot
        = [&candidates]() -> const auto& { return candidates; };
    ports.setActiveNavigationThumbnailRows
        = [&events](auto) { events.push_back(QStringLiteral("rows")); };
    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput input;
    input.session.documentKind = kiriview::DocumentSessionKind::Image;

    runtime.publish(input, {});
    ++candidates.revision;
    runtime.publish(input, {});

    QCOMPARE(events,
        (std::vector<QString> {
            QStringLiteral("commit"), QStringLiteral("rows"), QStringLiteral("rows") }));
}

void TestDocumentSessionProjectionRuntime::repeatedUnavailableThumbnailDependencyClearsRowsOnce()
{
    std::vector<QString> events;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&events](const auto&) {
        events.push_back(QStringLiteral("commit"));
        return true;
    };
    ports.setActiveNavigationThumbnailRows
        = [&events](auto) { events.push_back(QStringLiteral("rows")); };
    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));

    runtime.publish({}, {});
    runtime.publish({}, {});

    QCOMPARE(events, (std::vector<QString> { QStringLiteral("commit"), QStringLiteral("rows") }));
}

void TestDocumentSessionProjectionRuntime::
    mediaInformationRevisionDoesNotInvalidateSemanticDependency()
{
    int commitCount = 0;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&commitCount](const auto&) {
        ++commitCount;
        return true;
    };
    kiriview::DocumentSessionProjectionRuntime runtime(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput input;
    input.inputRevision = 1;
    input.session.documentKind = kiriview::DocumentSessionKind::Image;
    input.image.readyForInformation = true;
    input.image.displayedUrl = localUrl(QStringLiteral("/media/01.png"));
    input.image.directMediaSize = QSize(640, 480);
    input.image.embeddedMetadata.cameraModel = QStringLiteral("Camera");

    runtime.publish(input, {});
    input.inputRevision = 2;
    runtime.publish(input, {});

    QCOMPARE(commitCount, 1);

    input.image.directMediaSize = QSize(800, 600);
    runtime.publish(input, {});

    QCOMPARE(commitCount, 2);
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

void TestDocumentSessionProjectionRuntime::destructionDuringPublicSnapshotCommitStopsPublication()
{
    int thumbnailRowsCommitCount = 0;
    int revealCleanupCount = 0;
    std::unique_ptr<kiriview::DocumentSessionProjectionRuntime> runtime;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [&runtime](const auto&) {
        runtime.reset();
        return true;
    };
    ports.setActiveNavigationThumbnailRows
        = [&thumbnailRowsCommitCount](auto) { ++thumbnailRowsCommitCount; };
    ports.clearActiveNavigationRevealContextIfUnavailable
        = [&revealCleanupCount]() { ++revealCleanupCount; };
    runtime = std::make_unique<kiriview::DocumentSessionProjectionRuntime>(std::move(ports));

    runtime->publish({}, {});

    QVERIFY(!runtime);
    QCOMPARE(thumbnailRowsCommitCount, 0);
    QCOMPARE(revealCleanupCount, 0);
}

void TestDocumentSessionProjectionRuntime::nestedPublicationSupersedesOuterContinuation()
{
    const QUrl firstUrl = localUrl(QStringLiteral("/media/first.png"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/media/replacement.png"));
    std::vector<QUrl> committedUrls;
    int thumbnailRowsCommitCount = 0;
    int revealCleanupCount = 0;
    bool replacementSubmitted = false;
    std::unique_ptr<kiriview::DocumentSessionProjectionRuntime> runtime;
    kiriview::DocumentSessionPublicSnapshotInput replacementInput;
    replacementInput.session.sourceUrl = replacementUrl;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot
        = [&runtime, &replacementInput, &committedUrls, &replacementSubmitted](
              const kiriview::DocumentSessionPublicSnapshotInput& input) {
              committedUrls.push_back(input.session.sourceUrl);
              if (!replacementSubmitted) {
                  replacementSubmitted = true;
                  runtime->publish(replacementInput, {});
              }
              return true;
          };
    ports.setActiveNavigationThumbnailRows
        = [&thumbnailRowsCommitCount](auto) { ++thumbnailRowsCommitCount; };
    ports.clearActiveNavigationRevealContextIfUnavailable
        = [&revealCleanupCount]() { ++revealCleanupCount; };
    runtime = std::make_unique<kiriview::DocumentSessionProjectionRuntime>(std::move(ports));
    kiriview::DocumentSessionPublicSnapshotInput firstInput;
    firstInput.session.sourceUrl = firstUrl;

    runtime->publish(firstInput, {});
    runtime->publish(replacementInput, {});

    QCOMPARE(committedUrls, (std::vector<QUrl> { firstUrl, replacementUrl }));
    QCOMPARE(thumbnailRowsCommitCount, 1);
    QCOMPARE(revealCleanupCount, 1);
}

void TestDocumentSessionProjectionRuntime::destructionDuringThumbnailRowsCommitStopsPublication()
{
    const kiriview::ActiveNavigationSourceKind sourceKind
        = kiriview::ActiveNavigationSourceKind::OrdinaryDirectMedia;
    const kiriview::ActiveNavigationSnapshot navigation = activeNavigationSnapshot(1, 1);
    const kiriview::DirectMediaNavigationCandidateSnapshot candidates
        = directMediaNavigationCandidateSnapshot(
            { directMediaNavigationCandidate(localUrl(QStringLiteral("/media/01.png"))) });
    int revealCleanupCount = 0;
    std::unique_ptr<kiriview::DocumentSessionProjectionRuntime> runtime;
    kiriview::DocumentSessionProjectionRuntimePorts ports;
    ports.updatePublicSnapshot = [](const auto&) { return true; };
    ports.activeNavigationSourceKind = [&sourceKind]() { return sourceKind; };
    ports.activeNavigationSnapshot = [&navigation]() { return navigation; };
    ports.directMediaNavigationCandidateSnapshot
        = [&candidates]() -> const auto& { return candidates; };
    ports.setActiveNavigationThumbnailRows = [&runtime](auto) { runtime.reset(); };
    ports.clearActiveNavigationRevealContextIfUnavailable
        = [&revealCleanupCount]() { ++revealCleanupCount; };
    runtime = std::make_unique<kiriview::DocumentSessionProjectionRuntime>(std::move(ports));

    runtime->publish({}, {});

    QVERIFY(!runtime);
    QCOMPARE(revealCleanupCount, 0);
}

QTEST_GUILESS_MAIN(TestDocumentSessionProjectionRuntime)

#include "tst_documentsessionprojectionruntime.moc"
