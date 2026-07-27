// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "document/imagedocumentsourceloadscope.h"
#include "document/imagedocumentstate.h"
#include "document/imageopencontroller.h"
#include "location/imagedocumentlocation.h"

#include <QObject>
#include <QSize>
#include <QTest>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
QUrl localUrl(const QString& path) { return QUrl::fromLocalFile(path); }

kiriview::ImageDocumentSelectedTarget selectedTargetFor(
    const kiriview::ImageDocumentSourceLoadRequest& request)
{
    return {
        request.sourceUrl(),
        request.sourceKind(),
        kiriview::openedCollectionScopeForImageDocumentSourceLoad(request),
    };
}

kiriview::ImageDocumentSourceLoadRequest directImageRequest(const QUrl& url)
{
    return kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(url, {}));
}

kiriview::ImageDocumentSourceLoadRequest openedCollectionRequest(
    const QUrl& url, bool directorySource)
{
    kiriview::NavigationSourceEntryFacts facts;
    facts.requestedLocalSourceIsDirectory = directorySource;
    return kiriview::ImageDocumentSourceLoadRequest::fromExternalSource(
        kiriview::resolvedNavigationSource(url, facts));
}

kiriview::ImageDocumentPageCandidateListSnapshot pageCandidateListSnapshot(
    kiriview::ImageDocumentPageCandidateListSource source, const QUrl& pageUrl)
{
    kiriview::ImageDocumentPageCandidateListSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.revision = 1;
    snapshot.candidates = std::make_shared<const kiriview::ImageDocumentPageCandidateRows>(
        kiriview::ImageDocumentPageCandidateRows {
            kiriview::ImageDocumentPageCandidate { pageUrl, pageUrl.fileName() },
        });
    snapshot.known = true;
    return snapshot;
}

kiriview::ImageLoadFailure presentationFailure(
    const kiriview::ImageLoadSession& session, const QString& message)
{
    return {
        session.imageUrl(),
        session.id(),
        kiriview::ImageLoadFailureKind::Presentation,
        kiriview::DecodedImageFailureRoute::Unknown,
        kiriview::DecodedImageFailureOperation::Unknown,
        message,
        message,
        kiriview::ImageLoadFailureSeverity::Error,
        false,
    };
}

struct PageSlotCommit
{
    kiriview::DisplayedImageLocation location;
    QSize imageSize;
};

class OpenControllerFixture
{
public:
    OpenControllerFixture()
        : state([this](kiriview::ImageDocumentChange change) {
            stateChanges.push_back(change);
            if (stateChangeHook) {
                stateChangeHook(change);
            }
        })
    {
        kiriview::ImageOpenController::Callbacks callbacks;
        callbacks.findPredecodedImage
            = [](const QUrl&) { return std::optional<kiriview::PredecodedImage>(); };
        callbacks.runtimePlan = [this](const kiriview::ImageDocumentRuntimePlan& plan) {
            runtimePlans.push_back(plan);
        };
        callbacks.openedCollectionVideoPlaybackAvailable
            = [this](const kiriview::OpenedCollectionScopeLocation& scope, const QUrl& videoUrl) {
                  return videoPlaybackAvailability ? videoPlaybackAvailability(scope, videoUrl)
                                                   : false;
              };
        callbacks.commitPrimaryPageSlot
            = [this](const kiriview::DisplayedImageLocation& location, QSize imageSize) {
                  pageSlotCommits.push_back(PageSlotCommit { location, imageSize });
              };
        callbacks.invalidatePendingViewportImageLoad = [this]() {
            ++pendingViewportInvalidationCount;
            if (pendingViewportInvalidationHook) {
                pendingViewportInvalidationHook();
            }
        };
        callbacks.ensurePageCandidateSnapshot
            = [this](kiriview::ImageDocumentPageCandidateListContext context,
                  kiriview::ImageDocumentPageCandidateListSnapshotCallback callback) {
                  candidateContexts.push_back(std::move(context));
                  pendingCandidateSnapshot = std::move(callback);
              };
        callbacks.startViewportImageTarget = [this](kiriview::ImageLoadSession session) {
            startedSessions.push_back(std::move(session));
            return acceptViewportTargetStart;
        };
        callbacks.resolveViewportImageTarget
            = [this](kiriview::ImageLoadSession session, std::optional<kiriview::PredecodedImage>) {
                  resolvedSessions.push_back(std::move(session));
                  return acceptViewportTargetResolution;
              };
        callbacks.firstDisplayDecodeContext
            = []() { return kiriview::ImageFirstDisplayDecodeContext {}; };
        callbacks.hasAuthoritativeDisplay = [this]() { return hasAuthoritativeDisplay; };
        controller = std::make_unique<kiriview::ImageOpenController>(state, std::move(callbacks));
    }

    void openSource(const kiriview::ImageDocumentSourceLoadRequest& request)
    {
        controller->prepareSourceLoad(request);
        controller->cancel();
        state.setSelectedTarget(selectedTargetFor(request));
        controller->open();
    }

    std::vector<kiriview::ImageDocumentChange> stateChanges;
    std::function<void(kiriview::ImageDocumentChange)> stateChangeHook;
    kiriview::ImageDocumentState state;
    std::vector<kiriview::ImageDocumentRuntimePlan> runtimePlans;
    std::vector<PageSlotCommit> pageSlotCommits;
    std::vector<kiriview::ImageDocumentPageCandidateListContext> candidateContexts;
    std::optional<kiriview::ImageDocumentPageCandidateListSnapshotCallback>
        pendingCandidateSnapshot;
    std::vector<kiriview::ImageLoadSession> startedSessions;
    std::vector<kiriview::ImageLoadSession> resolvedSessions;
    std::size_t pendingViewportInvalidationCount = 0;
    std::function<void()> pendingViewportInvalidationHook;
    std::function<bool(const kiriview::OpenedCollectionScopeLocation&, const QUrl&)>
        videoPlaybackAvailability;
    bool acceptViewportTargetStart = true;
    bool acceptViewportTargetResolution = true;
    bool hasAuthoritativeDisplay = false;
    std::unique_ptr<kiriview::ImageOpenController> controller;
};
}

class TestImageOpenController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void staleViewportTerminalCannotPublishIntoReplacementLoad_data();
    void staleViewportTerminalCannotPublishIntoReplacementLoad();
    void currentViewportTerminalPublishesExactlyOnce();
    void currentViewportFailureUsesClaimedSessionIdentity();
    void failedViewportTargetSubmissionClaimsTerminalSession();
    void failedViewportTargetResolutionClaimsTerminalSession();
    void cancelInvalidatesSessionBeforePendingViewportCleanup();
    void reentrantLoadingNotificationCannotResumeSupersededOpen();
    void reentrantVideoAvailabilityProbeCannotPublishStaleVideoTerminal();
};

void TestImageOpenController::staleViewportTerminalCannotPublishIntoReplacementLoad_data()
{
    QTest::addColumn<bool>("directorySource");
    QTest::addColumn<bool>("readyTerminal");

    QTest::newRow("archive-ready") << false << true;
    QTest::newRow("archive-error") << false << false;
    QTest::newRow("directory-ready") << true << true;
    QTest::newRow("directory-error") << true << false;
}

void TestImageOpenController::staleViewportTerminalCannotPublishIntoReplacementLoad()
{
    QFETCH(bool, directorySource);
    QFETCH(bool, readyTerminal);

    OpenControllerFixture fixture;
    fixture.hasAuthoritativeDisplay = true;

    const QUrl retainedUrl = localUrl(QStringLiteral("/images/retained.png"));
    const kiriview::DisplayedImageLocation retainedLocation
        = kiriview::DisplayedImageLocation::fromUrl(retainedUrl);
    fixture.state.setSelectedTarget({ retainedUrl, kiriview::ImageDocumentPageKind::Image, {} });
    fixture.state.setDisplayedImageLocation(retainedLocation);
    fixture.state.setStatus(kiriview::ImageDocumentStatus::Ready);

    const QUrl staleUrl = localUrl(QStringLiteral("/images/pending.png"));
    fixture.openSource(directImageRequest(staleUrl));
    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    const kiriview::ImageLoadSession staleSession = fixture.resolvedSessions.front();

    const QUrl replacementUrl = directorySource
        ? localUrl(QStringLiteral("/collections/replacement/"))
        : localUrl(QStringLiteral("/collections/replacement.cbz"));
    const kiriview::ImageDocumentSourceLoadRequest replacementRequest
        = openedCollectionRequest(replacementUrl, directorySource);
    const kiriview::ImageDocumentSelectedTarget replacementTarget
        = selectedTargetFor(replacementRequest);
    fixture.openSource(replacementRequest);

    QVERIFY(fixture.pendingCandidateSnapshot.has_value());
    QCOMPARE(fixture.candidateContexts.size(), std::size_t(1));
    QCOMPARE(fixture.startedSessions.size(), std::size_t(2));
    QCOMPARE(fixture.startedSessions.back().imageUrl(), replacementUrl);
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    QVERIFY(fixture.state.selectedTarget() == replacementTarget);
    QVERIFY(fixture.state.displayedImageLocation() == retainedLocation);
    QVERIFY(fixture.state.loading());
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(fixture.state.embeddedMetadata().isEmpty());
    QVERIFY(fixture.state.errorString().isEmpty());
    QVERIFY(!fixture.state.loadFailure().has_value());
    QVERIFY(fixture.pageSlotCommits.empty());

    const std::size_t runtimePlanCount = fixture.runtimePlans.size();
    const quint64 lifecycleRevision = fixture.state.presentationLifecycleRevision();
    fixture.stateChanges.clear();

    if (readyTerminal) {
        kiriview::EmbeddedMetadata staleMetadata;
        staleMetadata.cameraMake = QStringLiteral("Stale Camera");
        fixture.controller->finishViewportImageLoadReady(
            staleSession, QSize(640, 480), std::move(staleMetadata));
    } else {
        fixture.controller->finishViewportImageLoadWithError(
            staleSession, presentationFailure(staleSession, QStringLiteral("stale failure")));
    }

    QVERIFY(fixture.state.selectedTarget() == replacementTarget);
    QVERIFY(fixture.state.displayedImageLocation() == retainedLocation);
    QVERIFY(fixture.state.loading());
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(fixture.state.embeddedMetadata().isEmpty());
    QVERIFY(fixture.state.errorString().isEmpty());
    QVERIFY(!fixture.state.loadFailure().has_value());
    QCOMPARE(fixture.state.presentationLifecycleRevision(), lifecycleRevision);
    QVERIFY(fixture.pageSlotCommits.empty());
    QCOMPARE(fixture.runtimePlans.size(), runtimePlanCount);
    QVERIFY(fixture.stateChanges.empty());
    QVERIFY(fixture.pendingCandidateSnapshot.has_value());

    QUrl replacementPageUrl = replacementTarget.openedCollectionScope.rootUrl();
    replacementPageUrl.setPath(replacementPageUrl.path() + QStringLiteral("01.png"));
    kiriview::ImageDocumentPageCandidateListSnapshotCallback replacementCompletion
        = std::move(*fixture.pendingCandidateSnapshot);
    fixture.pendingCandidateSnapshot.reset();
    replacementCompletion(kiriview::ImageDocumentPageCandidateListSnapshotResult {
        pageCandidateListSnapshot(fixture.candidateContexts.back().source(), replacementPageUrl),
        true,
        {},
    });

    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(2));
    QCOMPARE(fixture.resolvedSessions.back().imageUrl(), replacementPageUrl);
    QCOMPARE(fixture.resolvedSessions.back().openedCollectionScope(),
        replacementTarget.openedCollectionScope);
    QVERIFY(fixture.state.loading());
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Loading);
}

void TestImageOpenController::currentViewportTerminalPublishesExactlyOnce()
{
    OpenControllerFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/current.png"));
    fixture.openSource(directImageRequest(imageUrl));

    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    const kiriview::ImageLoadSession session = fixture.resolvedSessions.front();
    const std::size_t loadingRuntimePlanCount = fixture.runtimePlans.size();

    kiriview::EmbeddedMetadata firstMetadata;
    firstMetadata.cameraMake = QStringLiteral("First Camera");
    fixture.controller->finishViewportImageLoadReady(
        session, QSize(320, 200), std::move(firstMetadata));

    QCOMPARE(fixture.pageSlotCommits.size(), std::size_t(1));
    QVERIFY(fixture.pageSlotCommits.front().location == session.location());
    QCOMPARE(fixture.pageSlotCommits.front().imageSize, QSize(320, 200));
    QCOMPARE(fixture.runtimePlans.size(), loadingRuntimePlanCount + 1);
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Ready);
    QVERIFY(!fixture.state.loading());
    QVERIFY(fixture.state.displayedImageLocation() == session.location());
    QCOMPARE(fixture.state.embeddedMetadata().cameraMake, QStringLiteral("First Camera"));

    const std::size_t completedRuntimePlanCount = fixture.runtimePlans.size();
    const quint64 completedLifecycleRevision = fixture.state.presentationLifecycleRevision();
    fixture.stateChanges.clear();

    kiriview::EmbeddedMetadata duplicateMetadata;
    duplicateMetadata.cameraMake = QStringLiteral("Duplicate Camera");
    fixture.controller->finishViewportImageLoadReady(
        session, QSize(800, 600), std::move(duplicateMetadata));
    fixture.controller->finishViewportImageLoadWithError(
        session, presentationFailure(session, QStringLiteral("late failure")));

    QCOMPARE(fixture.pageSlotCommits.size(), std::size_t(1));
    QCOMPARE(fixture.runtimePlans.size(), completedRuntimePlanCount);
    QCOMPARE(fixture.state.presentationLifecycleRevision(), completedLifecycleRevision);
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Ready);
    QVERIFY(!fixture.state.loading());
    QVERIFY(fixture.state.displayedImageLocation() == session.location());
    QCOMPARE(fixture.state.embeddedMetadata().cameraMake, QStringLiteral("First Camera"));
    QVERIFY(fixture.state.errorString().isEmpty());
    QVERIFY(!fixture.state.loadFailure().has_value());
    QVERIFY(fixture.stateChanges.empty());
}

void TestImageOpenController::currentViewportFailureUsesClaimedSessionIdentity()
{
    OpenControllerFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/current-error.png"));
    fixture.openSource(directImageRequest(imageUrl));

    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    const kiriview::ImageLoadSession session = fixture.resolvedSessions.front();
    kiriview::ImageLoadFailure failure
        = presentationFailure(session, QStringLiteral("presentation failure"));
    failure.sourceUrl = localUrl(QStringLiteral("/images/forged.png"));
    failure.sessionId = session.id() + 1;

    fixture.controller->finishViewportImageLoadWithError(session, std::move(failure));

    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Error);
    QVERIFY(!fixture.state.loading());
    QVERIFY(fixture.state.loadFailure().has_value());
    QCOMPARE(fixture.state.loadFailure()->sourceUrl, imageUrl);
    QCOMPARE(fixture.state.loadFailure()->sessionId, session.id());
}

void TestImageOpenController::failedViewportTargetSubmissionClaimsTerminalSession()
{
    OpenControllerFixture fixture;
    fixture.acceptViewportTargetStart = false;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/unpresentable.png"));
    fixture.openSource(directImageRequest(imageUrl));

    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QVERIFY(fixture.resolvedSessions.empty());
    const kiriview::ImageLoadSession session = fixture.startedSessions.front();
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Error);
    QVERIFY(!fixture.state.loading());
    QVERIFY(fixture.state.displayedImageLocation().isEmpty());
    QVERIFY(fixture.state.loadFailure().has_value());
    QCOMPARE(fixture.state.loadFailure()->sourceUrl, imageUrl);
    QCOMPARE(fixture.state.loadFailure()->sessionId, session.id());
    QVERIFY(fixture.pageSlotCommits.empty());

    const std::size_t completedRuntimePlanCount = fixture.runtimePlans.size();
    const quint64 completedLifecycleRevision = fixture.state.presentationLifecycleRevision();
    const QString completedErrorString = fixture.state.errorString();
    fixture.stateChanges.clear();

    fixture.controller->finishViewportImageLoadReady(
        session, QSize(400, 300), kiriview::EmbeddedMetadata {});

    QVERIFY(fixture.pageSlotCommits.empty());
    QCOMPARE(fixture.runtimePlans.size(), completedRuntimePlanCount);
    QCOMPARE(fixture.state.presentationLifecycleRevision(), completedLifecycleRevision);
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Error);
    QVERIFY(!fixture.state.loading());
    QVERIFY(fixture.state.displayedImageLocation().isEmpty());
    QCOMPARE(fixture.state.errorString(), completedErrorString);
    QVERIFY(fixture.state.loadFailure().has_value());
    QCOMPARE(fixture.state.loadFailure()->sourceUrl, imageUrl);
    QCOMPARE(fixture.state.loadFailure()->sessionId, session.id());
    QVERIFY(fixture.stateChanges.empty());
}

void TestImageOpenController::failedViewportTargetResolutionClaimsTerminalSession()
{
    OpenControllerFixture fixture;
    fixture.acceptViewportTargetResolution = false;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/unresolvable.png"));
    fixture.openSource(directImageRequest(imageUrl));

    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    const kiriview::ImageLoadSession session = fixture.resolvedSessions.front();
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Error);
    QVERIFY(!fixture.state.loading());
    QVERIFY(fixture.state.displayedImageLocation().isEmpty());
    QVERIFY(fixture.state.loadFailure().has_value());
    QCOMPARE(fixture.state.loadFailure()->sourceUrl, imageUrl);
    QCOMPARE(fixture.state.loadFailure()->sessionId, session.id());
    QVERIFY(fixture.pageSlotCommits.empty());
}

void TestImageOpenController::cancelInvalidatesSessionBeforePendingViewportCleanup()
{
    OpenControllerFixture fixture;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/cancelled.png"));
    fixture.openSource(directImageRequest(imageUrl));

    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    const kiriview::ImageLoadSession session = fixture.resolvedSessions.front();
    const std::size_t runtimePlanCount = fixture.runtimePlans.size();
    const std::size_t invalidationCount = fixture.pendingViewportInvalidationCount;
    fixture.stateChanges.clear();
    fixture.pendingViewportInvalidationHook = [&fixture, session]() {
        fixture.controller->finishViewportImageLoadReady(
            session, QSize(640, 480), kiriview::EmbeddedMetadata {});
    };

    fixture.controller->cancel();

    QCOMPARE(fixture.pendingViewportInvalidationCount, invalidationCount + 1);
    QVERIFY(fixture.pageSlotCommits.empty());
    QCOMPARE(fixture.runtimePlans.size(), runtimePlanCount);
    QVERIFY(fixture.state.loading());
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(fixture.state.displayedImageLocation().isEmpty());
    QVERIFY(fixture.stateChanges.empty());
}

void TestImageOpenController::reentrantLoadingNotificationCannotResumeSupersededOpen()
{
    OpenControllerFixture fixture;
    const QUrl staleUrl = localUrl(QStringLiteral("/images/reentrant-stale.png"));
    const QUrl replacementUrl = localUrl(QStringLiteral("/images/reentrant-replacement.png"));
    const kiriview::ImageDocumentSourceLoadRequest replacementRequest
        = directImageRequest(replacementUrl);
    bool replacementStarted = false;
    fixture.stateChangeHook = [&fixture, replacementRequest, staleUrl, &replacementStarted](
                                  kiriview::ImageDocumentChange change) {
        if (replacementStarted || change != kiriview::ImageDocumentChange::Loading
            || fixture.state.sourceUrl() != staleUrl) {
            return;
        }
        replacementStarted = true;
        fixture.openSource(replacementRequest);
    };

    fixture.openSource(directImageRequest(staleUrl));

    QVERIFY(replacementStarted);
    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.startedSessions.front().imageUrl(), replacementUrl);
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.front().imageUrl(), replacementUrl);
    QCOMPARE(fixture.state.sourceUrl(), replacementUrl);
    QVERIFY(fixture.state.loading());
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Loading);
    QCOMPARE(fixture.runtimePlans.size(), std::size_t(1));
}

void TestImageOpenController::reentrantVideoAvailabilityProbeCannotPublishStaleVideoTerminal()
{
    OpenControllerFixture fixture;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/video.cbz"));
    const std::optional<kiriview::OpenedCollectionScopeLocation> archiveCollection
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
            kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(archiveCollection.has_value());
    QUrl videoUrl = archiveCollection->rootUrl();
    videoUrl.setPath(videoUrl.path() + QStringLiteral("01.mp4"));
    const kiriview::ImageDocumentSourceLoadRequest videoRequest
        = kiriview::ImageDocumentSourceLoadRequest::fromSameScopePageTarget(
            kiriview::ImageDocumentPageTarget {
                videoUrl,
                kiriview::ImageDocumentPageKind::Video,
            },
            *archiveCollection);
    const QUrl replacementUrl = localUrl(QStringLiteral("/images/replacement.png"));
    const kiriview::ImageDocumentSourceLoadRequest replacementRequest
        = directImageRequest(replacementUrl);
    const kiriview::ImageDocumentSelectedTarget replacementTarget
        = selectedTargetFor(replacementRequest);
    int probeCount = 0;
    bool replacementStarted = false;
    fixture.videoPlaybackAvailability
        = [&fixture, replacementRequest, &probeCount, &replacementStarted](
              const kiriview::OpenedCollectionScopeLocation&, const QUrl&) {
              ++probeCount;
              if (!replacementStarted) {
                  replacementStarted = true;
                  fixture.openSource(replacementRequest);
              }
              return false;
          };

    fixture.openSource(videoRequest);

    QCOMPARE(probeCount, 1);
    QCOMPARE(fixture.startedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.startedSessions.front().imageUrl(), replacementUrl);
    QCOMPARE(fixture.resolvedSessions.size(), std::size_t(1));
    QCOMPARE(fixture.resolvedSessions.front().imageUrl(), replacementUrl);
    QVERIFY(fixture.state.selectedTarget() == replacementTarget);
    QVERIFY(fixture.state.loading());
    QCOMPARE(fixture.state.status(), kiriview::ImageDocumentStatus::Loading);
    QVERIFY(!fixture.state.unsupportedOpenedCollectionVideo());
    QVERIFY(fixture.state.displayedImageLocation().isEmpty());
}

QTEST_GUILESS_MAIN(TestImageOpenController)

#include "tst_imageopencontroller.moc"
