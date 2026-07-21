// SPDX-FileCopyrightText: 2026 KIM Hyunjae
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "candidate_test_support.h"
#include "document/imageloadsessiontracker.h"
#include "location/imagedocumentlocation.h"
#include "location/imageurl.h"

#include <QObject>
#include <QSize>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <limits>
#include <optional>
#include <vector>

namespace {
using kiriview::TestSupport::archivePageUrl;
using kiriview::TestSupport::imageDocumentPageCandidate;
using kiriview::TestSupport::localUrl;
using kiriview::TestSupport::videoCandidate;

struct DirectArchiveCase
{
    QString fileName;
    kiriview::OpenedCollectionScopeKind kind;
    QString rootScheme;
};

std::vector<DirectArchiveCase> directArchiveCases()
{
    return {
        { QStringLiteral("book.cbz"), kiriview::OpenedCollectionScopeKind::ComicBookArchive,
            QStringLiteral("zip") },
        { QStringLiteral("book.cbt"), kiriview::OpenedCollectionScopeKind::ComicBookArchive,
            QStringLiteral("tar") },
        { QStringLiteral("book.cb7"), kiriview::OpenedCollectionScopeKind::ComicBookArchive,
            QStringLiteral("sevenz") },
        { QStringLiteral("book.cbr"), kiriview::OpenedCollectionScopeKind::ComicBookArchive,
            QStringLiteral("rar") },
        { QStringLiteral("book.zip"), kiriview::OpenedCollectionScopeKind::GeneralArchive,
            QStringLiteral("zip") },
        { QStringLiteral("book.tar"), kiriview::OpenedCollectionScopeKind::GeneralArchive,
            QStringLiteral("tar") },
        { QStringLiteral("book.7z"), kiriview::OpenedCollectionScopeKind::GeneralArchive,
            QStringLiteral("sevenz") },
        { QStringLiteral("book.rar"), kiriview::OpenedCollectionScopeKind::GeneralArchive,
            QStringLiteral("rar") },
    };
}
}

class TestImageLoadSessionTracker : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void startOwnsSessionIdAndFirstDisplayContext();
    void directlyOpenedArchiveFormatsStartOpenedCollectionCandidateLoad();
    void directlyOpenedDirectoryStartsOpenedCollectionCandidateLoad();
    void openedCollectionScopeRetainsResolvedNavigationSourceFacts();
    void sameScopePageLoadDoesNotProbeAgain();
    void staleSessionsCannotResolveOrFinishCurrentLoad();
    void archiveResolutionUpdatesCanonicalCurrentSession();
    void archiveResolutionReportsUnsupportedOpenedCollectionVideo();
    void archiveResolutionUsesCandidateKindInsteadOfExtension();
    void emptyOpenedCollectionResolutionClaimsCurrentSessionForError();
    void claimCurrentClearsTheActiveSession();
    void sessionIdsStayNonZeroAfterWrap();
};

void TestImageLoadSessionTracker::startOwnsSessionIdAndFirstDisplayContext()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));

    const kiriview::ImageLoadPlan firstPlan
        = tracker.start(kiriview::ImageLoadRequest::fromExternalSource(
                            kiriview::resolvedNavigationSource(firstUrl, {})),
            kiriview::ImageFirstDisplayDecodeContext { QSize(320, 240) });
    const kiriview::ImageLoadPlan secondPlan
        = tracker.start(kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(secondUrl, {})));

    QCOMPARE(firstPlan.session.id(), quint64(1));
    QCOMPARE(secondPlan.session.id(), quint64(2));
    QCOMPARE(firstPlan.session.firstDisplay().logicalViewportSize, QSize(320, 240));
    QCOMPARE(secondPlan.session.firstDisplay().logicalViewportSize, QSize());
    QVERIFY(!tracker.isCurrent(firstPlan.session));
    QVERIFY(tracker.isCurrent(secondPlan.session));
}

void TestImageLoadSessionTracker::directlyOpenedArchiveFormatsStartOpenedCollectionCandidateLoad()
{
    for (const DirectArchiveCase& archiveCase : directArchiveCases()) {
        kiriview::ImageLoadSessionTracker tracker;
        const QUrl archiveUrl = localUrl(QStringLiteral("/books/") + archiveCase.fileName);

        const kiriview::ImageLoadPlan plan
            = tracker.start(kiriview::ImageLoadRequest::fromExternalSource(
                kiriview::resolvedNavigationSource(archiveUrl, {})));
        const kiriview::OpenedCollectionScopeLocation& scope = plan.session.openedCollectionScope();

        QCOMPARE(
            plan.startEffect, kiriview::ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates);
        QCOMPARE(plan.session.imageUrl(), archiveUrl);
        QVERIFY(!scope.isEmpty());
        QCOMPARE(scope.fileUrl(), archiveUrl);
        QCOMPARE(scope.kind(), archiveCase.kind);
        QCOMPARE(scope.rootUrl().scheme(), archiveCase.rootScheme);
        QCOMPARE(scope.isComicBook(),
            archiveCase.kind == kiriview::OpenedCollectionScopeKind::ComicBookArchive);
        QVERIFY(tracker.isCurrent(plan.session));
    }
}

void TestImageLoadSessionTracker::directlyOpenedDirectoryStartsOpenedCollectionCandidateLoad()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QUrl directoryUrl = localUrl(directory.path());

    kiriview::ImageLoadSessionTracker tracker;
    const kiriview::ImageLoadPlan plan
        = tracker.start(kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::NavigationSourceResolver().resolveExternalSource(directoryUrl)));
    const kiriview::OpenedCollectionScopeLocation& scope = plan.session.openedCollectionScope();

    QCOMPARE(plan.startEffect, kiriview::ImageLoadStartEffect::LoadOpenedCollectionScopeCandidates);
    QCOMPARE(plan.session.imageUrl(), directoryUrl);
    QVERIFY(!scope.isEmpty());
    QVERIFY(scope.isDirectory());
    QCOMPARE(scope.fileUrl(), kiriview::normalizedFileContainerUrl(directoryUrl));
    QCOMPARE(scope.rootUrl(), kiriview::normalizedDirectoryContainerUrl(directoryUrl));
    QCOMPARE(scope.kind(), kiriview::OpenedCollectionScopeKind::Directory);
    QVERIFY(tracker.isCurrent(plan.session));
}

void TestImageLoadSessionTracker::openedCollectionScopeRetainsResolvedNavigationSourceFacts()
{
    int probeCount = 0;
    const kiriview::NavigationSourceResolver resolver([&probeCount](const QUrl&) {
        ++probeCount;
        return kiriview::NavigationSourceEntryFacts {};
    });
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl archiveUrl = QUrl::fromLocalFile(QStringLiteral("/books/book.cbz"));

    const kiriview::ImageLoadPlan first = tracker.start(
        kiriview::ImageLoadRequest::fromExternalSource(resolver.resolveExternalSource(archiveUrl)));
    QCOMPARE(probeCount, 1);
    QCOMPARE(first.session.openedCollectionScope().navigationSourceUrl(), archiveUrl);
    QCOMPARE(first.session.openedCollectionScope().navigationSourceUrl(), archiveUrl);
    QCOMPARE(first.session.openedCollectionScope().navigationSourceUrl(), archiveUrl);
    QCOMPARE(probeCount, 1);

    tracker.start(
        kiriview::ImageLoadRequest::fromExternalSource(resolver.resolveExternalSource(archiveUrl)));
    QCOMPARE(probeCount, 2);
}

void TestImageLoadSessionTracker::sameScopePageLoadDoesNotProbeAgain()
{
    int probeCount = 0;
    const kiriview::NavigationSourceEntryFactProvider provider = [&probeCount](const QUrl&) {
        ++probeCount;
        return kiriview::NavigationSourceEntryFacts {};
    };
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const kiriview::ResolvedNavigationSource source
        = kiriview::NavigationSourceResolver(provider).resolveExternalSource(archiveUrl);
    const std::optional<kiriview::OpenedCollectionScopeLocation> scope
        = kiriview::openedCollectionScopeLocationForLocalArchiveSource(source);
    QVERIFY(scope.has_value());
    const QUrl pageUrl = archivePageUrl(scope->rootUrl(), QStringLiteral("02.png"));
    QCOMPARE(probeCount, 1);

    tracker.start(kiriview::ImageLoadRequest::fromSameScopePageTarget(
        kiriview::ImageDocumentPageTarget(pageUrl, kiriview::ImageDocumentPageKind::Image), *scope,
        false));

    QCOMPARE(probeCount, 1);
}

void TestImageLoadSessionTracker::staleSessionsCannotResolveOrFinishCurrentLoad()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl firstUrl = localUrl(QStringLiteral("/images/01.png"));
    const QUrl secondUrl = localUrl(QStringLiteral("/images/02.png"));

    const kiriview::ImageLoadSession staleSession
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                  kiriview::resolvedNavigationSource(firstUrl, {})))
              .session;
    const kiriview::ImageLoadSession currentSession
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                  kiriview::resolvedNavigationSource(secondUrl, {})))
              .session;

    const kiriview::OpenedCollectionCandidateCompletion staleArchiveCompletion
        = tracker.completeOpenedCollectionCandidates(
            staleSession, { imageDocumentPageCandidate(firstUrl) });
    QCOMPARE(staleArchiveCompletion.action,
        kiriview::OpenedCollectionCandidateCompletionAction::Ignored);
    QVERIFY(!tracker.claimCurrent(staleSession).has_value());
    QVERIFY(tracker.isCurrent(currentSession));
}

void TestImageLoadSessionTracker::archiveResolutionUpdatesCanonicalCurrentSession()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const auto collection = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
        kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(collection.has_value());
    const QUrl imageUrl = archivePageUrl(collection->rootUrl(), QStringLiteral("01.png"));

    const kiriview::ImageLoadSession session
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                         kiriview::resolvedNavigationSource(archiveUrl, {})),
                  kiriview::ImageFirstDisplayDecodeContext { QSize(320, 240) })
              .session;
    const kiriview::OpenedCollectionCandidateCompletion completion
        = tracker.completeOpenedCollectionCandidates(
            session, { imageDocumentPageCandidate(imageUrl) });

    QCOMPARE(
        completion.action, kiriview::OpenedCollectionCandidateCompletionAction::StartImageDecode);
    const kiriview::ImageLoadSession& resolvedSession = completion.session;
    QCOMPARE(resolvedSession.imageUrl(), imageUrl);
    QCOMPARE(resolvedSession.kind(), kiriview::ImageDocumentPageKind::Image);
    QCOMPARE(resolvedSession.firstDisplay().logicalViewportSize, QSize(320, 240));
    QVERIFY(tracker.isCurrent(resolvedSession));
}

void TestImageLoadSessionTracker::archiveResolutionReportsUnsupportedOpenedCollectionVideo()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const auto collection = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
        kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(collection.has_value());
    const QUrl videoUrl = archivePageUrl(collection->rootUrl(), QStringLiteral("01.bin"));

    const kiriview::ImageLoadSession session
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                  kiriview::resolvedNavigationSource(archiveUrl, {})))
              .session;
    const kiriview::OpenedCollectionCandidateCompletion completion
        = tracker.completeOpenedCollectionCandidates(session, { videoCandidate(videoUrl) });

    QCOMPARE(completion.action,
        kiriview::OpenedCollectionCandidateCompletionAction::
            ReportUnsupportedOpenedCollectionVideo);
    QCOMPARE(completion.session.imageUrl(), videoUrl);
    QCOMPARE(completion.session.kind(), kiriview::ImageDocumentPageKind::Video);
    QVERIFY(!tracker.isCurrent(session));
}

void TestImageLoadSessionTracker::archiveResolutionUsesCandidateKindInsteadOfExtension()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));
    const auto collection = kiriview::openedCollectionScopeLocationForLocalArchiveSource(
        kiriview::resolvedNavigationSource(archiveUrl, {}));
    QVERIFY(collection.has_value());
    const QUrl imageUrl = archivePageUrl(collection->rootUrl(), QStringLiteral("01.mp4"));

    const kiriview::ImageLoadSession session
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                  kiriview::resolvedNavigationSource(archiveUrl, {})))
              .session;
    const kiriview::OpenedCollectionCandidateCompletion completion
        = tracker.completeOpenedCollectionCandidates(
            session, { imageDocumentPageCandidate(imageUrl) });

    QCOMPARE(
        completion.action, kiriview::OpenedCollectionCandidateCompletionAction::StartImageDecode);
    QCOMPARE(completion.session.imageUrl(), imageUrl);
    QCOMPARE(completion.session.kind(), kiriview::ImageDocumentPageKind::Image);
}

void TestImageLoadSessionTracker::emptyOpenedCollectionResolutionClaimsCurrentSessionForError()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl archiveUrl = localUrl(QStringLiteral("/books/book.cbz"));

    const kiriview::ImageLoadSession session
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                  kiriview::resolvedNavigationSource(archiveUrl, {})))
              .session;

    const kiriview::OpenedCollectionCandidateCompletion completion
        = tracker.completeOpenedCollectionCandidates(session, {});

    QCOMPARE(completion.action,
        kiriview::OpenedCollectionCandidateCompletionAction::ReportEmptyOpenedCollection);
    QCOMPARE(completion.session.imageUrl(), archiveUrl);
    QVERIFY(!tracker.isCurrent(session));
}

void TestImageLoadSessionTracker::claimCurrentClearsTheActiveSession()
{
    kiriview::ImageLoadSessionTracker tracker;
    const QUrl imageUrl = localUrl(QStringLiteral("/images/01.png"));
    const kiriview::ImageLoadSession session
        = tracker
              .start(kiriview::ImageLoadRequest::fromExternalSource(
                  kiriview::resolvedNavigationSource(imageUrl, {})))
              .session;

    const std::optional<kiriview::ImageLoadSession> takenSession = tracker.claimCurrent(session);

    QVERIFY(takenSession.has_value());
    QCOMPARE(takenSession->imageUrl(), imageUrl);
    QVERIFY(!tracker.isCurrent(session));
}

void TestImageLoadSessionTracker::sessionIdsStayNonZeroAfterWrap()
{
    kiriview::ImageLoadSessionTracker tracker(std::numeric_limits<quint64>::max());

    const kiriview::ImageLoadPlan firstPlan
        = tracker.start(kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(localUrl(QStringLiteral("/1.png")), {})));
    const kiriview::ImageLoadPlan secondPlan
        = tracker.start(kiriview::ImageLoadRequest::fromExternalSource(
            kiriview::resolvedNavigationSource(localUrl(QStringLiteral("/2.png")), {})));

    QCOMPARE(firstPlan.session.id(), quint64(1));
    QCOMPARE(secondPlan.session.id(), quint64(2));
}

QTEST_GUILESS_MAIN(TestImageLoadSessionTracker)

#include "tst_imageloadsessiontracker.moc"
